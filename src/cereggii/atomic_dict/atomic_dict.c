// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <stdatomic.h>
#include "atomic_dict.h"
#include "atomic_ref.h"
#include "pythread.h"

PyObject *
AtomicDict_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AtomicDict *self;
    self = (AtomicDict *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->metadata = (AtomicRef *) AtomicRef_new(&AtomicRef_Type, NULL, NULL);
        if (self->metadata == NULL)
            goto fail;
        AtomicRef_init(self->metadata, NULL, NULL);

        self->new_gen_metadata = (AtomicRef *) AtomicRef_new(&AtomicRef_Type, NULL, NULL);
        if (self->new_gen_metadata == NULL)
            goto fail;
        AtomicRef_init(self->new_gen_metadata, NULL, NULL);

        self->reservation_buffer_size = 0;
        Py_tss_t *tss_key = PyThread_tss_alloc();
        if (tss_key == NULL)
            goto fail;
        if (PyThread_tss_create(tss_key))
            goto fail;
        assert(PyThread_tss_is_created(tss_key) != 0);
        self->tss_key = tss_key;

        self->reservation_buffers = Py_BuildValue("[]");
        if (self->reservation_buffers == NULL)
            goto fail;
    }
    return (PyObject *) self;

    fail:
    Py_XDECREF(self->metadata);
    Py_XDECREF(self->new_gen_metadata);
    Py_XDECREF(self);
    return NULL;
}

int
AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    assert(atomic_ref_get_ref(self->metadata) == Py_None);
    assert(atomic_ref_get_ref(self->new_gen_metadata) == Py_None);
    long init_dict_size = 0;
    long initial_size = 0;
    long buffer_size = 4;
    PyObject *init_dict = NULL;

    if (args != NULL) {
        if (!PyArg_ParseTuple(args, "|O", &init_dict)) {
            goto fail;
        }
        if (init_dict != NULL) {
            if (!PyDict_Check(init_dict)) {
                PyErr_SetString(PyExc_TypeError, "initial iterable must be a dict.");
                goto fail;
            }
        }
    }

    if (kwargs != NULL) {
        // it is unnecessary to acquire the GIL/begin a critical section:
        // this is the only reference to kwargs
        PyObject *initial_size_arg = PyDict_GetItemString(kwargs, "initial_size");
        if (initial_size_arg != NULL) {
            initial_size = PyLong_AsLong(initial_size_arg);
            PyDict_DelItemString(kwargs, "initial_size");
            if (initial_size > (1UL << ATOMIC_DICT_MAX_LOG_SIZE)) {
                PyErr_SetString(PyExc_ValueError, "initial_size can be at most 2^56.");
                return -1;
            }
        }
        PyObject *buffer_size_arg = PyDict_GetItemString(kwargs, "buffer_size");
        if (buffer_size_arg != NULL) {
            buffer_size = PyLong_AsLong(buffer_size_arg);
            PyDict_DelItemString(kwargs, "buffer_size");
            if (buffer_size != 1 && buffer_size != 2 && buffer_size != 4 &&
                buffer_size != 8 && buffer_size != 16 && buffer_size != 32 &&
                buffer_size != 64) {
                PyErr_SetString(PyExc_ValueError, "buffer_size must be a power of 2, between 1 and 64.");
                return -1;
            }
        }

        if (init_dict == NULL) {
            init_dict = kwargs;
        } else {
            // this internally calls Py_BEGIN_CRITICAL_SECTION
            PyDict_Update(init_dict, kwargs);
        }
    }

    if (init_dict != NULL) {
        init_dict_size = PyDict_Size(init_dict);
    }

    // max(initial_size, init_dict_size, 64)
    if (initial_size < init_dict_size) {
        initial_size = init_dict_size;
    }
    if (initial_size < 64) {
        initial_size = 64;
    }

    self->reservation_buffer_size = buffer_size;

    unsigned char log_size = 0;
    unsigned long initial_size_tmp = (unsigned long) initial_size;
    while (initial_size_tmp >>= 1) {
        log_size++;
    }
    if (initial_size > 1 << log_size) {
        log_size++;
    }
    // 64 = 0b1000000
    // 0 -> 0b1000000
    // 1 -> 0b100000
    // 2 -> 0b10000
    // 3 -> 0b1000
    // 4 -> 0b100
    // 5 -> 0b10
    // 6 -> 0b1
    // 7 -> 0b0
    if (log_size > ATOMIC_DICT_MAX_LOG_SIZE) {
        PyErr_SetString(PyExc_ValueError, "can hold at most 2^56 items.");
        return -1;
    }

    atomic_dict_meta *meta;
    create:
    meta = atomic_dict_new_meta(log_size, NULL);
    if (meta == NULL)
        goto fail;
    atomic_ref_set_ref(self->metadata, (PyObject *) meta);

    atomic_dict_block *block;
    long i;
    for (i = 0; i < initial_size / 64; i++) {
        // allocate blocks
        block = atomic_dict_block_new(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> 6) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }
    if (initial_size % 64 > 0) {
        // allocate additional block
        block = atomic_dict_block_new(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> 6) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }

    meta->inserting_block = 0;

    if (init_dict != NULL) {
        PyObject *key, *value;
        Py_hash_t hash;
        Py_ssize_t pos = 0;
        while (PyDict_Next(init_dict, &pos, &key, &value)) {
            hash = PyObject_Hash(key);
            if (hash == -1)
                goto fail;

            if (AtomicDict_UnsafeInsert(self, key, hash, value, pos - 1) == -1) {
                Py_DECREF(meta);
                log_size++;
                goto create;
            }
        }
        meta->inserting_block = pos >> 6;
    }

    // handle possibly misaligned reservations on last block
    // => put them into this thread's reservation buffer
    Py_XDECREF(init_dict);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_XDECREF(init_dict);
    return -1;
}

void
AtomicDict_dealloc(AtomicDict *self)
{
    PyObject_GC_UnTrack(self);
    atomic_dict_meta *meta = (atomic_dict_meta *) atomic_ref_get_ref(self->metadata);
    PyMem_RawFree(meta->blocks);
    Py_DECREF(meta); // decref for the above atomic_ref_get_ref
    Py_CLEAR(self->metadata);
    Py_CLEAR(self->new_gen_metadata);
    Py_CLEAR(self->reservation_buffers);
    // this should be enough to deallocate the reservation buffers themselves as well:
    // the list should be the only reference to them anyway
    PyThread_tss_delete(self->tss_key);
    PyThread_tss_free(self->tss_key);
    // clear this dict's elements (iter XXX)
    Py_TYPE(self)->tp_free((PyObject *) self);
}

int
AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg)
{
    Py_VISIT(self->metadata);
    Py_VISIT(self->new_gen_metadata);
    // traverse this dict's elements (iter XXX)
    return 0;
}

/**
 * previous_blocks may be NULL.
 */
atomic_dict_meta *
atomic_dict_new_meta(unsigned char log_size, atomic_dict_meta *previous_meta)
{
    node_size_info node_sizes = node_sizes_table[log_size];

    PyObject *generation = PyObject_CallObject((PyObject *) &PyBaseObject_Type, NULL);
    if (generation == NULL)
        goto fail;

    unsigned long *index = PyMem_RawMalloc(node_sizes.node_size * (1 << log_size));
    if (index == NULL)
        goto fail;
    memset(index, 0, node_sizes.node_size * (1 << log_size));

    atomic_dict_block **previous_blocks;
    long inserting_block;
    long greatest_allocated_block;
    long greatest_deleted_block;
    long greatest_refilled_block;
    if (previous_meta != NULL) {
        previous_blocks = previous_meta->blocks;
        inserting_block = previous_meta->inserting_block;
        greatest_allocated_block = previous_meta->greatest_allocated_block;
        greatest_deleted_block = previous_meta->greatest_deleted_block;
        greatest_refilled_block = previous_meta->greatest_refilled_block;
    } else {
        previous_blocks = NULL;
        inserting_block = -1;
        greatest_allocated_block = -1;
        greatest_deleted_block = -1;
        greatest_refilled_block = -1;
    }

    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    atomic_dict_block **blocks = PyMem_RawRealloc(previous_blocks,
                                                  sizeof(atomic_dict_block *) * ((1 << log_size) >> 6));
    if (blocks == NULL)
        goto fail;

    if (previous_blocks == NULL) {
        blocks[0] = NULL;
    } else if (greatest_allocated_block == (1 << log_size) >> 6) {
        blocks[greatest_allocated_block + 1] = NULL;
    }

    atomic_dict_meta *meta = PyObject_New(atomic_dict_meta, &AtomicDictMeta);
    if (meta == NULL)
        goto fail;
    PyObject_Init((PyObject *) meta, &AtomicDictMeta);

    meta->log_size = log_size;
    meta->generation = generation;
    meta->index = index;
    meta->blocks = blocks;
    meta->node_size = node_sizes.node_size;
    meta->distance_size = node_sizes.distance_size;
    meta->tag_size = node_sizes.tag_size;
    meta->nodes_in_region = 8 / (meta->node_size / 8);
    meta->nodes_in_two_regions = 16 / (meta->node_size / 8);
    meta->node_mask = (1UL << node_sizes.node_size) - 1;
    meta->index_mask = ((1UL << log_size) - 1) << (node_sizes.node_size - log_size);
    meta->distance_mask = ((1UL << node_sizes.distance_size) - 1) << node_sizes.tag_size;
    meta->tag_mask = (1UL << node_sizes.tag_size) - 1;
    switch (node_sizes.node_size) {
        case 8:
            meta->shift_mask = 8 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_8_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_16_nodes_at;
            break;
        case 16:
            meta->shift_mask = 4 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_4_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_8_nodes_at;
            break;
        case 32:
            meta->shift_mask = 2 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_2_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_4_nodes_at;
            break;
        case 64:
            meta->shift_mask = 1 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_1_node_at;
            meta->read_double_word_nodes_at = atomic_dict_read_2_nodes_at;
            break;
    }

    meta->inserting_block = inserting_block;
    meta->greatest_allocated_block = greatest_allocated_block;
    meta->greatest_deleted_block = greatest_deleted_block;
    meta->greatest_refilled_block = greatest_refilled_block;

    return meta;
    fail:
    Py_XDECREF(generation);
    Py_XDECREF(meta);
    PyMem_RawFree(index);
    PyMem_RawFree(blocks);
    return NULL;
}

void
atomic_dict_meta_dealloc(atomic_dict_meta *self)
{
    // not gc tracked (?)
    PyMem_RawFree(self->index);
    Py_CLEAR(self->generation);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

atomic_dict_block *
atomic_dict_block_new(atomic_dict_meta *meta)
{
    atomic_dict_block *new = PyMem_RawMalloc(sizeof(atomic_dict_block));

    if (new == NULL)
        return NULL;

    new->generation = meta->generation;
    memset(new->entries, 0, sizeof(atomic_dict_entry) * 64);

    return new;
}

void
AtomicDict_Search(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                  int look_into_reservations, atomic_dict_search_result *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    unsigned long ix = hash & ((1 << meta->log_size) - 1);
    int previous_distance = -1;  // = inf
    int probe;
    int reservations = 0;

    for (probe = 0; probe < meta->log_size; probe++) {
        atomic_dict_read_node_at((ix + probe + reservations) % (1 << meta->log_size), &result->node, meta);

        if (atomic_dict_node_is_reservation(&result->node, meta)) {
            probe--;
            reservations++;
            if (look_into_reservations) {
                result->is_reservation = 1;
                goto check_entry;
            } else {
                continue;
            }
        }
        result->is_reservation = 0;

        if (result->node.node == 0) {
            goto not_found;
        }

        if (result->node.distance <= previous_distance) {
            goto not_found;
        }
        previous_distance = result->node.distance;

        if (result->node.tag == (hash & meta->tag_mask)) {
            check_entry:
            result->entry_p = &(meta
                ->blocks[result->node.index >> 6]
                ->entries[result->node.index & 63]);
            result->entry = *result->entry_p; // READ

            if (result->entry.flags & ENTRY_FLAGS_TOMBSTONE) {
                continue;
            }
            if (result->entry.key == key) {
                goto found;
            }
            if (result->entry.hash != hash) {
                continue;
            }
            int cmp = PyObject_RichCompareBool(result->entry.key, key, Py_EQ);
            if (cmp < 0) {
                // exception thrown during compare
                goto error;
            } else if (cmp == 0) {
                continue;
            }
            goto found;
        }
    }  // probes exhausted

    not_found:
    result->error = 0;
    result->entry_p = NULL;
    return;
    error:
    result->error = 1;
    return;
    found:
    result->error = 0;
    result->index = ix;
}

PyObject *
AtomicDict_GetItem(AtomicDict *self, PyObject *key)
{
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    atomic_dict_search_result result;
    atomic_dict_meta *meta;
    retry:
    meta = (atomic_dict_meta *) atomic_ref_get_ref(self->metadata);

    result.entry.value = NULL;
    AtomicDict_Search(meta, key, hash, 0, &result);
    if (result.error)
        goto fail;

    if (atomic_ref_get_ref(self->metadata) != (PyObject *) meta) {
        Py_DECREF(meta);
        goto retry;
    }
    Py_DECREF(meta); // for atomic_ref_get_ref

    if (result.entry_p == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
        result.entry.value = NULL;
    }

    Py_DECREF(meta);
    return result.entry.value;
    fail:
    Py_XDECREF(meta);
    return NULL;
}

/**
 * This is not thread-safe!
 *
 * Used at initialization time, when there can be no concurrent access.
 * Doesn't allocate blocks, nor check for migrations, nor update dk->metadata refcount.
 * Doesn't do updates: repeated keys will be repeated, so make sure successive
 * calls to this function don't try to insert the same key into the same AtomicDict.
 **/
int
AtomicDict_UnsafeInsert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos)
{
    atomic_dict_meta meta;
    meta = *(atomic_dict_meta *) atomic_ref_get_ref(self->metadata);
    // pos === node_index
    atomic_dict_block *block = meta.blocks[pos >> 6];
    block->entries[pos & 63].flags = ENTRY_FLAGS_RESERVED;
    block->entries[pos & 63].hash = hash;
    block->entries[pos & 63].key = key;
    block->entries[pos & 63].value = value;

    atomic_dict_node temp;
    atomic_dict_node node = {
        .index = pos,
        .tag = hash,
    };
    unsigned long ix = hash & ((1 << meta.log_size) - 1);

    for (int probe = 0; probe < (1 << meta.distance_size); probe++) {
        atomic_dict_read_node_at(ix + probe, &temp, &meta);

        if (temp.node == 0) {
            node.distance = probe;
            atomic_dict_write_node_at(ix + probe, &node, &meta);
            goto done;
        }

        if (temp.distance < probe) {
            // non-atomic robin hood
            node.distance = probe;
            unsigned long i = ix + probe;
            atomic_dict_write_node_at(i, &node, &meta);
            node = temp;
            i++;
            while (temp.node != 0) { // until first empty slot
                atomic_dict_read_node_at(i, &temp, &meta);
                if (node.distance > temp.distance) {
                    atomic_dict_write_node_at(i, &node, &meta);
                    node = temp;
                }
                i++;
            }
            atomic_dict_write_node_at(i, &temp, &meta);
            goto done;
        }
    }
    // probes exhausted
    return -1;
    done:
    return 0;
}

void
atomic_dict_get_empty_entry(AtomicDict *dk, atomic_dict_meta *meta, atomic_dict_reservation_buffer *rb,
                            atomic_dict_entry_loc *entry_loc, Py_hash_t hash)
{
    atomic_dict_reservation_buffer_pop(rb, entry_loc);

    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & 63 & ~(dk->reservation_buffer_size - 1);
        long inserting_block;

        reserve_in_inserting_block:
        inserting_block = meta->inserting_block;
        for (int offset = 0; offset < 64; offset += dk->reservation_buffer_size) {
            entry_loc->entry = &meta
                ->blocks[inserting_block]
                ->entries[(insert_position + offset) % 64];
            if (entry_loc->entry->flags == 0) {
                if (__sync_bool_compare_and_swap_1(&entry_loc->entry->flags, 0, ENTRY_FLAGS_RESERVED)) {
                    entry_loc->location = insert_position + offset;
                    atomic_dict_reservation_buffer_put(rb, entry_loc, dk->reservation_buffer_size);
                    atomic_dict_reservation_buffer_pop(rb, entry_loc);
                    goto done;
                }
            }
        }

        if (meta->inserting_block != inserting_block)
            goto reserve_in_inserting_block;

        long greatest_allocated_block = meta->greatest_allocated_block;
        if (greatest_allocated_block > inserting_block) {
            _Py_atomic_compare_exchange_int64(&meta->inserting_block, inserting_block, inserting_block + 1);
            goto reserve_in_inserting_block; // even if the above CAS fails
        }
        // if (greatest_allocated_block + 1 > (1 << meta->log_size) >> 6) {
        //     grow();
        // }
        assert(greatest_allocated_block + 1 <= (1 << meta->log_size) >> 6);

        atomic_dict_block *block = atomic_dict_block_new(meta);
        if (block == NULL)
            goto fail;
        block->entries[0].flags = ENTRY_FLAGS_RESERVED;

        if (_Py_atomic_compare_exchange_ptr(&meta->blocks[greatest_allocated_block + 1], NULL, block)) {
            if (greatest_allocated_block + 2 < (1 << meta->log_size) >> 6) {
                meta->blocks[greatest_allocated_block + 2] = NULL;
            }
            assert(_Py_atomic_compare_exchange_int64(&meta->greatest_allocated_block,
                                                     greatest_allocated_block,
                                                     greatest_allocated_block + 1));
            assert(_Py_atomic_compare_exchange_int64(&meta->inserting_block,
                                                     greatest_allocated_block,
                                                     greatest_allocated_block + 1));
            entry_loc->entry = &block->entries[0];
            entry_loc->location = (meta->greatest_allocated_block + 1) << 6;
            atomic_dict_reservation_buffer_put(rb, entry_loc, dk->reservation_buffer_size);
        } else {
            PyMem_RawFree(block);
        }
        goto reserve_in_inserting_block;
    }

    done:
    assert(entry_loc->entry != NULL);
    return;
    fail:
    entry_loc->entry = NULL;
}


typedef enum atomic_dict_robin_hood_result {
    ok,
    failed,
    grow,
} atomic_dict_robin_hood_result;

atomic_dict_robin_hood_result
atomic_dict_robin_hood(atomic_dict_meta *meta, atomic_dict_node *nodes, atomic_dict_node to_insert, int distance_0_ix)
{
    /*
     * Lythe and listin, gentilmen,
     * That be of frebore blode;
     * I shall you tel of a gode yeman,
     * His name was Robyn Hode.
     * */

    /*
     * assumptions:
     *   1. there are 16 / (meta->node_size / 8) valid nodes in `nodes`
     *   2. there is at least 1 empty node
     * */

    atomic_dict_node current = to_insert;
    atomic_dict_node temp;
    int probe = 0;
    int reservations = 0;
    int cursor;

    beginning:
    for (; probe < meta->nodes_in_two_regions; probe++) {
        if (probe >= (1 << meta->distance_size) - 1) {
            return grow;
        }
        cursor = distance_0_ix + probe + reservations;
        if (nodes[cursor].node == 0) {
            current.distance = probe;
            nodes[cursor] = current;
            return ok;
        }

        if (atomic_dict_node_is_reservation(&nodes[cursor], meta)) {
            probe--;
            reservations++;
//            if (look_into_reservations) {
//                result->is_reservation = 1;
//                goto check_entry;
//            } else {
//                continue;
//            }
        }

        if (nodes[cursor].distance < probe) {
            current.distance = probe;
            atomic_dict_compute_raw_node(&current, meta);
            temp = nodes[cursor];
            nodes[cursor] = current;
            current = temp;
            distance_0_ix = cursor - temp.distance;
            probe = temp.distance;
            goto beginning;
        }
    }
    return failed;
}

void
atomic_dict_robin_hood_reservation(atomic_dict_meta *meta, atomic_dict_node *nodes, int distance_0_ix)
{
    /*
     * assumptions:
     *   1. there are 16 / (meta->node_size / 8) valid nodes in `nodes`
     *   2. the right-most node is a reservation node
     *   3. distance_0_ix is the distance-0 index (i.e. the ideal index)
     *      for the right-most reservation node
     * */

    int nodes_in_two_words = 16 / (meta->node_size / 8);
    atomic_dict_node current = nodes[nodes_in_two_words - 1];
    atomic_dict_node temp;
    int probe = 0;
    int reservations = 0;

    beginning:
    for (; probe < nodes_in_two_words; probe++) {
        if (nodes[probe + reservations].node == 0) {
            current.distance = probe;
            nodes[probe + reservations] = current;
            return;
        }

        if (nodes[probe + reservations].distance < probe) {
            current.distance = probe;
            temp = nodes[probe + reservations];
            nodes[probe + reservations] = current;
            current = temp;
            goto beginning;
        }
    }
}

void
atomic_dict_nodes_copy_buffers(atomic_dict_node *from_buffer, atomic_dict_node *to_buffer)
{
    for (int i = 0; i < 16; ++i) {
        to_buffer[i] = from_buffer[i];
    }
}

int
atomic_dict_insert_entry(atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc, Py_hash_t hash)
{
    atomic_dict_node read_buffer[16]; // can read at most 16 nodes at a time
    atomic_dict_node temp[16];
    atomic_dict_node node = {
        .index = entry_loc->location,
        .tag = hash,
    };
    atomic_dict_node reservation = {
        .index = entry_loc->location,
        .distance = (1 << meta->distance_size) - 1,
        .tag = 0,
    };

    unsigned long ix = hash & ((1 << meta->log_size) - 1);

    beginning:
    meta->read_double_word_nodes_at(ix & ~63, read_buffer, meta);

    int start = (int) ix % meta->nodes_in_two_regions;

    for (int i = start; i < meta->nodes_in_two_regions; ++i) {
        if (read_buffer[i].node == 0) {
            atomic_dict_nodes_copy_buffers(read_buffer, temp);
            atomic_dict_robin_hood_result rh = atomic_dict_robin_hood(meta, temp, node, start);
            // if (rh == grow) {
            //     atomic_dict_grow();
            // }
            assert(rh == ok);
            int begin_write = -1;
            for (int j = start; j < meta->nodes_in_two_regions; ++j) {
                atomic_dict_compute_raw_node(&temp[j], meta);
                if (temp[j].node != read_buffer[j].node) {
                    begin_write = j;
                    break;
                }
            }
            assert(begin_write != -1);
            int end_write;
            for (int j = begin_write + 1; j < meta->nodes_in_two_regions; ++j) {
                atomic_dict_compute_raw_node(&temp[j], meta);
                if (temp[j].node == read_buffer[j].node) {
                    end_write = j;
                    break;
                }
            }
            assert(end_write > begin_write);
            if (atomic_dict_atomic_write_nodes_at(ix + begin_write - start, end_write - begin_write,
                                                  &read_buffer[begin_write], &temp[begin_write], meta)) {
                goto done;
            }
            goto beginning;
        }
        // check reservations
    }

    int cursor = meta->nodes_in_two_regions - 1;

    int reservation_offset;
    for (reservation_offset = meta->nodes_in_two_regions;
         reservation_offset < 1 << meta->log_size; ++reservation_offset) {
        cursor++;
        cursor = cursor % meta->nodes_in_two_regions;
        if (cursor == 0) {
            meta->read_double_word_nodes_at(ix + reservation_offset, read_buffer, meta);
        }

        if (read_buffer[cursor].node == 0) {
            if (atomic_dict_atomic_write_node_at(ix + reservation_offset, &read_buffer[cursor], &reservation, meta)) {
                goto reserved;
            }
            goto beginning;
        }
        // check reservations
    }

    reserved:
    // 1. help reservations on the left
    // 2. proceed with my reservation

    done:
    return 0;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    assert(key != NULL);
    assert(value != NULL);

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1) {
        return -1;
    }

    atomic_dict_reservation_buffer *rb = atomic_dict_get_reservation_buffer(self);
    if (rb == NULL)
        goto fail;

    atomic_dict_meta *meta;
    meta = (atomic_dict_meta *) atomic_ref_get_ref(self->metadata);

    atomic_dict_search_result sr;
    search_for_update:
    AtomicDict_Search(meta, key, hash, 0, &sr);
    if (sr.error)
        goto fail;

    Py_INCREF(value);
    if (sr.entry_p != NULL) {
        if (_Py_atomic_compare_exchange_ptr(&sr.entry_p->value, sr.entry.value, value)) {
            Py_DECREF(sr.entry.value);
            goto done;
        }
        goto search_for_update;
    }

    atomic_dict_entry_loc entry_loc;
    atomic_dict_get_empty_entry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL)
        goto fail;

    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;
    atomic_dict_insert_entry(meta, &entry_loc, hash);

    done:
    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(value);
    return -1;
}

PyObject *
AtomicDict_Debug(AtomicDict *self)
{
    atomic_dict_meta meta;
    meta = *(atomic_dict_meta *) atomic_ref_get_ref(self->metadata);
    PyObject *metadata = Py_BuildValue("{sOsOsOsOsOsOsOsOsOsOsOsOsO}",
                                       "log_size\0", Py_BuildValue("B", meta.log_size),
                                       "generation\0", Py_BuildValue("O", meta.generation),
                                       "node_size\0", Py_BuildValue("B", meta.node_size),
                                       "distance_size\0", Py_BuildValue("B", meta.distance_size),
                                       "tag_size\0", Py_BuildValue("B", meta.tag_size),
                                       "node_mask\0", Py_BuildValue("k", meta.node_mask),
                                       "index_mask\0", Py_BuildValue("k", meta.index_mask),
                                       "distance_mask\0", Py_BuildValue("k", meta.distance_mask),
                                       "tag_mask\0", Py_BuildValue("k", meta.tag_mask),
                                       "inserting_block\0", Py_BuildValue("l", meta.inserting_block),
                                       "greatest_allocated_block\0", Py_BuildValue("l", meta.greatest_allocated_block),
                                       "greatest_deleted_block\0", Py_BuildValue("l", meta.greatest_deleted_block),
                                       "greatest_refilled_block\0", Py_BuildValue("l", meta.greatest_refilled_block));
    if (metadata == NULL)
        goto fail;

    PyObject *index_nodes = Py_BuildValue("[]");
    if (index_nodes == NULL)
        goto fail;

    atomic_dict_node node;
    for (unsigned long i = 0; i < (1 << meta.log_size); i++) {
        atomic_dict_read_node_at(i, &node, &meta);
        PyObject *n = Py_BuildValue("k", node.node);
        if (n == NULL)
            goto fail;
        PyList_Append(index_nodes, n);
        Py_DECREF(n);
    }

    PyObject *blocks = Py_BuildValue("[]");
    if (blocks == NULL)
        goto fail;

    atomic_dict_block *block;
    PyObject *entries = NULL;
    PyObject *entry_tuple = NULL;
    PyObject *block_info = NULL;
    for (unsigned long i = 0; i <= meta.greatest_allocated_block; i++) {
        block = meta.blocks[i];
        entries = Py_BuildValue("[]");
        if (entries == NULL)
            goto fail;

        for (int j = 0; j < 64; j++) {
            if (block->entries[j].key != NULL) {
                entry_tuple = Py_BuildValue("(BlOO)",
                                            block->entries[j].flags,
                                            block->entries[j].hash,
                                            block->entries[j].key,
                                            block->entries[j].value);
                if (entry_tuple == NULL)
                    goto fail;
                PyList_Append(entries, entry_tuple);
                Py_DECREF(entry_tuple);
            }
        }

        block_info = Py_BuildValue("{sOsO}",
                                   "gen\0", block->generation,
                                   "entries\0", entries);
        Py_DECREF(entries);
        if (block_info == NULL)
            goto fail;
        PyList_Append(blocks, block_info);
        Py_DECREF(block_info);
    }

    PyObject *out = Py_BuildValue("{sOsOsO}", "meta\0", metadata, "blocks\0", blocks, "index\0", index_nodes);
    if (out == NULL)
        goto fail;
    Py_DECREF(metadata);
    Py_DECREF(blocks);
    Py_DECREF(index_nodes);
    return out;

    fail:
    PyErr_SetString(PyExc_RuntimeError, "unable to get debug info");
    Py_XDECREF(metadata);
    Py_XDECREF(index_nodes);
    Py_XDECREF(blocks);
    Py_XDECREF(entries);
    Py_XDECREF(entry_tuple);
    Py_XDECREF(block_info);
    return NULL;
}
