// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


PyObject *
AtomicDict_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
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
    assert(AtomicRef_Get(self->metadata) == Py_None);
    assert(AtomicRef_Get(self->new_gen_metadata) == Py_None);
    int64_t init_dict_size = 0;
    int64_t min_size = 0;
    int64_t buffer_size = 4;
    PyObject *init_dict = NULL;
    atomic_dict_meta *meta = NULL;

    if (args != NULL) {
        if (!PyArg_ParseTuple(args, "|O", &init_dict)) {
            goto fail;
        }
        if (init_dict != NULL) {
            if (!PyDict_Check(init_dict)) {
                PyErr_SetString(PyExc_TypeError, "type(iterable) is not dict");
                goto fail;
            }
        }
    }

    if (kwargs != NULL) {
        // it is unnecessary to acquire the GIL/begin a critical section:
        // this is the only reference to kwargs
        PyObject *min_size_arg = PyDict_GetItemString(kwargs, "min_size");
        if (min_size_arg != NULL) {
            min_size = PyLong_AsLong(min_size_arg);
            PyDict_DelItemString(kwargs, "min_size");
            if (min_size > (1UL << ATOMIC_DICT_MAX_LOG_SIZE)) {
                PyErr_SetString(PyExc_ValueError, "min_size > 2 ** 56");
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
                PyErr_SetString(PyExc_ValueError, "buffer_size not in [1, 2, 4, 8, 16, 32, 64]");
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
    if (init_dict_size % 64 == 0) { // allocate one more entry: cannot write to entry 0
        init_dict_size++;
    }
    if (min_size < 64) {
        min_size = 64;
    }

    self->reservation_buffer_size = buffer_size;

    uint8_t log_size = 0, init_dict_log_size = 0;
    uint64_t min_size_tmp = (uint64_t) min_size;
    uint64_t init_dict_size_tmp = (uint64_t) init_dict_size;
    while (min_size_tmp >>= 1) {
        log_size++;
    }
    if (min_size > 1 << log_size) {
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
    self->min_log_size = log_size;

    while (init_dict_size_tmp >>= 1) {
        init_dict_log_size++;
    }
    if (init_dict_size > 1 << init_dict_log_size) {
        init_dict_log_size++;
    }
    if (init_dict_log_size > log_size) {
        log_size = init_dict_log_size;
    }

    create:
    meta = AtomicDict_NewMeta(log_size, NULL);
    if (meta == NULL)
        goto fail;
    AtomicRef_Set(self->metadata, (PyObject *) meta);

    atomic_dict_block *block;
    int64_t i;
    for (i = 0; i < init_dict_size / 64; i++) {
        // allocate blocks
        block = AtomicDict_NewBlock(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> 6) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }
    if (init_dict_size % 64 > 0) {
        // allocate additional block
        block = AtomicDict_NewBlock(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> 6) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }

    meta->inserting_block = 0;
    atomic_dict_reservation_buffer *rb;
    atomic_dict_entry_loc entry_loc;

    if (init_dict != NULL && PyDict_Size(init_dict) > 0) {
        PyObject *key, *value;
        Py_hash_t hash;
        Py_ssize_t pos = 0;

        while (PyDict_Next(init_dict, &pos, &key, &value)) {
            hash = PyObject_Hash(key);
            if (hash == -1)
                goto fail;

            int inserted = AtomicDict_UnsafeInsert(self, key, hash, value, pos); // we want to avoid pos = 0
            if (inserted == -1) {
                Py_DECREF(meta);
                log_size++;
                goto create;
            }
        }
        meta->inserting_block = pos >> 6;

        if (pos > 0) {
            rb = AtomicDict_GetReservationBuffer(self);
            if (rb == NULL)
                goto fail;

            // handle possibly misaligned reservations on last block
            // => put them into this thread's reservation buffer
            entry_loc.entry = &meta->blocks[(pos + 1) >> 6]->entries[(pos + 1) & 63];
            entry_loc.location = pos + 1;
            uint8_t n = self->reservation_buffer_size - (uint8_t) (entry_loc.location % self->reservation_buffer_size);
            if (n > 0) {
                AtomicDict_ReservationBufferPut(rb, &entry_loc, n);
            }
        }
    }

    if (!(meta->blocks[0]->entries[0].flags & ENTRY_FLAGS_RESERVED)) {
        rb = AtomicDict_GetReservationBuffer(self);
        if (rb == NULL)
            goto fail;

        // mark entry 0 as reserved and put the remaining entries
        // into this thread's reservation buffer
        meta->blocks[0]->entries[0].flags |= ENTRY_FLAGS_RESERVED;
        for (i = 1; i < self->reservation_buffer_size; ++i) {
            entry_loc.entry = &meta->blocks[0]->entries[i & 63];
            entry_loc.location = i;
            if (entry_loc.entry->key == NULL) {
                int found = 0;
                for (int j = 0; j < RESERVATION_BUFFER_SIZE; ++j) {
                    if (rb->reservations[j].location == entry_loc.location) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    AtomicDict_ReservationBufferPut(rb, &entry_loc, 1);
                }
            }
        }
    }

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
    atomic_dict_meta *meta = (atomic_dict_meta *) AtomicRef_Get(self->metadata);
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
    meta = *(atomic_dict_meta *) AtomicRef_Get(self->metadata);
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
    uint64_t ix = hash & ((1 << meta.log_size) - 1);

    for (int probe = 0; probe < (1 << meta.distance_size); probe++) {
        AtomicDict_ReadNodeAt(ix + probe, &temp, &meta);

        if (temp.node == 0) {
            node.distance = probe;
            AtomicDict_WriteNodeAt(ix + probe, &node, &meta);
            goto done;
        }

        if (temp.distance < probe) {
            // non-atomic robin hood
            node.distance = probe;
            uint64_t i = ix + probe;
            AtomicDict_WriteNodeAt(i, &node, &meta);
            node = temp;
            i++;
            while (temp.node != 0) { // until first empty slot
                AtomicDict_ReadNodeAt(i, &temp, &meta);
                if (node.distance > temp.distance) {
                    AtomicDict_WriteNodeAt(i, &node, &meta);
                    node = temp;
                }
                i++;
            }
            AtomicDict_WriteNodeAt(i, &temp, &meta);
            goto done;
        }
    }
    // probes exhausted
    return -1;
    done:
    Py_INCREF(key);
    Py_INCREF(value);
    return 0;
}

PyObject *
AtomicDict_Debug(AtomicDict *self)
{
    atomic_dict_meta meta;
    meta = *(atomic_dict_meta *) AtomicRef_Get(self->metadata);
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
    for (uint64_t i = 0; i < (1 << meta.log_size); i++) {
        AtomicDict_ReadNodeAt(i, &node, &meta);
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
    for (uint64_t i = 0; i <= meta.greatest_allocated_block; i++) {
        block = meta.blocks[i];
        entries = Py_BuildValue("[]");
        if (entries == NULL)
            goto fail;

        for (int j = 0; j < 64; j++) {
            PyObject *key = block->entries[j].key;
            if (key != NULL) {
                PyObject *value = block->entries[j].value;
                entry_tuple = Py_BuildValue("(BlOO)",
                                            block->entries[j].flags,
                                            block->entries[j].hash,
                                            key,
                                            value);
                if (entry_tuple == NULL)
                    goto fail;
                Py_INCREF(key);
                Py_INCREF(value);
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
