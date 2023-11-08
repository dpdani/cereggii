// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_ref.h"

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
    log_size++;
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
    int i;
    for (i = 0; i < initial_size >> 6; i++) {
        // allocate blocks
        block = atomic_dict_block_new(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        meta->greatest_allocated_block++;
    }
    if (initial_size % 64 > 0) {
        // allocate additional block
        block = atomic_dict_block_new(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        meta->greatest_allocated_block++;
    }

    if (init_dict != NULL) {
        PyObject *key, *value;
        Py_hash_t hash;
        Py_ssize_t pos = 0;
        while (PyDict_Next(init_dict, &pos, &key, &value)) {
            hash = PyObject_Hash(key);
            if (hash == -1)
                goto fail;

            if (atomic_dict_unsafe_insert(self, key, hash, value, pos - 1) == -1) {
                Py_DECREF(meta);
                log_size++;
                goto create;
            }
        }
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
    long greatest_allocated_block;
    long greatest_deleted_block;
    long greatest_refilled_block;
    if (previous_meta != NULL) {
        previous_blocks = previous_meta->blocks;
        greatest_allocated_block = previous_meta->greatest_allocated_block;
        greatest_deleted_block = previous_meta->greatest_deleted_block;
        greatest_refilled_block = previous_meta->greatest_refilled_block;
    } else {
        previous_blocks = NULL;
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
    meta->node_mask = (1UL << node_sizes.node_size) - 1;
    meta->index_mask = ((1UL << log_size) - 1) << (node_sizes.node_size - log_size);
    meta->distance_mask = ((1UL << node_sizes.distance_size) - 1) << node_sizes.tag_size;
    meta->tag_mask = (1UL << node_sizes.tag_size) - 1;
    switch (node_sizes.node_size) {
        case 8:
            meta->shift_mask = 8 - 1;
        case 16:
            meta->shift_mask = 4 - 1;
        case 32:
            meta->shift_mask = 2 - 1;
        case 64:
            meta->shift_mask = 1 - 1;
    }

    meta->greatest_allocated_block = greatest_allocated_block;
    meta->greatest_deleted_block = greatest_deleted_block;
    meta->greatest_refilled_block = greatest_refilled_block;

    atomic_dict_node reserved = {
        .index = 1,
        .distance = (1 << meta->distance_size) - 1,  // so that ~d = 0
        .tag = 0,
    };
    atomic_dict_compute_raw_node(&reserved, meta);
    meta->reserved_node = reserved.node;

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
atomic_dict_search(AtomicDict *dk, atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                   atomic_dict_search_result *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    unsigned long ix = hash & ((1 << meta->log_size) - 1);
    int previous_distance = -1;  // = inf
    int probe;
    int reservations = 0;

    for (probe = 0; probe < meta->log_size; probe++) {
        atomic_dict_read_node_at(ix + probe + reservations, &result->node, meta);

        if (result->node.node == meta->reserved_node) {
            probe--;
            reservations++;
            continue;
        }

        if (result->node.node == 0) {
            goto not_found;
        }

        if (result->node.distance <= previous_distance) {
            goto not_found;
        }
        previous_distance = result->node.distance;

        if (result->node.tag == (hash & meta->tag_mask)) {
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
atomic_dict_lookup(AtomicDict *self, PyObject *key)
{
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    atomic_dict_search_result result;
    atomic_dict_meta *meta;
    retry:
    meta = (atomic_dict_meta *) atomic_ref_get_ref(self->metadata);

    result.entry.value = NULL;
    atomic_dict_search(self, meta, key, hash, &result);
    if (result.error)
        goto fail;

    if (atomic_ref_get_ref(self->metadata) != (PyObject *) meta) {
        Py_DECREF(meta);
        goto retry;
    }

    if (result.entry_p == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
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
atomic_dict_unsafe_insert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos)
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

    for (int probe = 0; probe < meta.log_size; probe++) {
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

int
atomic_dict_insert_or_update(AtomicDict *dk, PyObject *key, PyObject *value)
{
    return 0;
}

PyObject *
atomic_dict_debug(AtomicDict *self)
{
    atomic_dict_meta meta;
    meta = *(atomic_dict_meta *) atomic_ref_get_ref(self->metadata);
    PyObject *metadata = Py_BuildValue("{sOsOsOsOsOsOsOsOsO}",
                                       "log_size\0", Py_BuildValue("B", meta.log_size),
                                       "generation\0", Py_BuildValue("O", meta.generation),
                                       "node_size\0", Py_BuildValue("B", meta.node_size),
                                       "distance_size\0", Py_BuildValue("B", meta.distance_size),
                                       "tag_size\0", Py_BuildValue("B", meta.tag_size),
                                       "node_mask\0", Py_BuildValue("k", meta.node_mask),
                                       "index_mask\0", Py_BuildValue("k", meta.index_mask),
                                       "distance_mask\0", Py_BuildValue("k", meta.distance_mask),
                                       "tag_mask\0", Py_BuildValue("k", meta.tag_mask),
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
    PyObject *entries = Py_BuildValue("[]");
    if (entries == NULL)
        goto fail;
    PyObject *entry_tuple = NULL;
    PyObject *block_info = NULL;
    for (unsigned long i = 0; i < meta.greatest_allocated_block; i++) {
        block = meta.blocks[i];

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
