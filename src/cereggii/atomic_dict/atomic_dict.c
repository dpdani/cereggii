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
    AtomicDict *self = NULL;
    self = PyObject_GC_New(AtomicDict, &AtomicDict_Type);
    if (self != NULL) {
        self->metadata = NULL;
        self->metadata = (AtomicRef *) AtomicRef_new(&AtomicRef_Type, NULL, NULL);
        if (self->metadata == NULL)
            goto fail;
        AtomicRef_init(self->metadata, NULL, NULL);

        self->reservation_buffer_size = 0;
        Py_tss_t *tss_key = NULL;
        tss_key = PyThread_tss_alloc();
        if (tss_key == NULL)
            goto fail;
        if (PyThread_tss_create(tss_key))
            goto fail;
        assert(PyThread_tss_is_created(tss_key) != 0);
        self->accessor_key = tss_key;

        self->accessors = NULL;
        self->accessors = Py_BuildValue("[]");
        if (self->accessors == NULL)
            goto fail;

        self->accessors_lock = (PyMutex) {0};

        PyObject_GC_Track(self);
    }
    return (PyObject *) self;

    fail:
    Py_XDECREF(self->metadata);
    Py_XDECREF(self->accessors);
    Py_XDECREF(self);
    return NULL;
}

int
AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    assert(AtomicRef_Get(self->metadata) == Py_None);
    int64_t init_dict_size = 0;
    int64_t min_size = 0;
    int64_t buffer_size = 4;
    PyObject *initial = NULL;
    PyObject *min_size_arg = NULL;
    PyObject *buffer_size_arg = NULL;
    AtomicDict_Meta *meta = NULL;

    char *kw_list[] = {"initial", "min_size", "buffer_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO", kw_list, &initial, &min_size_arg, &buffer_size_arg)) {
        goto fail;
    }
    if (initial != NULL) {
        if (!PyDict_Check(initial)) {
            PyErr_SetString(PyExc_TypeError, "type(initial) is not dict");
            goto fail;
        }
    }

    if (min_size_arg != NULL) {
        min_size = PyLong_AsLong(min_size_arg);
        if (min_size > (1UL << ATOMIC_DICT_MAX_LOG_SIZE)) {
            PyErr_SetString(PyExc_ValueError, "min_size > 2 ** 56");
            return -1;
        }
    }
    if (buffer_size_arg != NULL) {
        buffer_size = PyLong_AsLong(buffer_size_arg);
        if (buffer_size != 1 && buffer_size != 2 && buffer_size != 4 &&
            buffer_size != 8 && buffer_size != 16 && buffer_size != 32 &&
            buffer_size != 64) {
            PyErr_SetString(PyExc_ValueError, "buffer_size not in (1, 2, 4, 8, 16, 32, 64)");
            return -1;
        }
    }

    if (initial != NULL) {
        init_dict_size = PyDict_Size(initial) * 2;
    }
    if (init_dict_size % ATOMIC_DICT_ENTRIES_IN_BLOCK == 0) { // allocate one more entry: cannot write to entry 0
        init_dict_size++;
    }
    if (min_size < ATOMIC_DICT_ENTRIES_IN_BLOCK) {
        min_size = ATOMIC_DICT_ENTRIES_IN_BLOCK;
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
    meta = NULL;
    meta = AtomicDictMeta_New(log_size);
    if (meta == NULL)
        goto fail;
    AtomicDictMeta_ClearIndex(meta);
    if (AtomicDictMeta_InitBlocks(meta) < 0)
        goto fail;
    AtomicRef_Set(self->metadata, (PyObject *) meta);

    AtomicDict_Block *block;
    int64_t i;
    for (i = 0; i < init_dict_size / ATOMIC_DICT_ENTRIES_IN_BLOCK; i++) {
        // allocate blocks
        block = AtomicDictBlock_New(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }
    if (init_dict_size % ATOMIC_DICT_ENTRIES_IN_BLOCK > 0) {
        // allocate additional block
        block = AtomicDictBlock_New(meta);
        if (block == NULL)
            goto fail;
        meta->blocks[i] = block;
        if (i + 1 < (1 << log_size) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
            meta->blocks[i + 1] = NULL;
        }
        meta->greatest_allocated_block++;
    }

    meta->inserting_block = 0;
    AtomicDict_AccessorStorage *storage;
    AtomicDict_EntryLoc entry_loc;
    self->sync_op = (PyMutex) {0};
    self->accessors_lock = (PyMutex) {0};
    self->len = 0;
    self->len_dirty = 0;

    if (initial != NULL && PyDict_Size(initial) > 0) {
        PyObject *key, *value;
        Py_hash_t hash;
        Py_ssize_t pos = 0;

        Py_BEGIN_CRITICAL_SECTION(initial);

        while (PyDict_Next(initial, &pos, &key, &value)) {
            hash = PyObject_Hash(key);
            if (hash == -1)
                goto fail;

            self->len++; // we want to avoid pos = 0
            AtomicDict_Entry *entry = AtomicDict_GetEntryAt(self->len, meta);
            Py_INCREF(key);
            Py_INCREF(value);
            entry->flags = ENTRY_FLAGS_RESERVED;
            entry->hash = hash;
            entry->key = key;
            entry->value = value;
            int inserted = AtomicDict_UnsafeInsert(meta, hash, self->len);
            if (inserted == -1) {
                Py_DECREF(meta);
                log_size++;
                goto create;
            }
        }

        Py_END_CRITICAL_SECTION();

        meta->inserting_block = self->len >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK;

        if (self->len > 0) {
            storage = AtomicDict_GetOrCreateAccessorStorage(self);
            if (storage == NULL)
                goto fail;

            // handle possibly misaligned reservations on last block
            // => put them into this thread's reservation buffer
            if (AtomicDict_BlockOf(self->len + 1) <= meta->greatest_allocated_block) {
                entry_loc.entry = AtomicDict_GetEntryAt(self->len + 1, meta);
                entry_loc.location = self->len + 1;

                uint8_t n =
                    self->reservation_buffer_size - (uint8_t) (entry_loc.location % self->reservation_buffer_size);
                assert(n <= ATOMIC_DICT_ENTRIES_IN_BLOCK);
                while (AtomicDict_BlockOf(self->len + n) > meta->greatest_allocated_block) {
                    n--;

                    if (n == 0)
                        break;
                }

                if (n > 0) {
                    AtomicDict_ReservationBufferPut(&storage->reservation_buffer, &entry_loc, n);
                }
            }
        }
    }

    if (!(AtomicDict_GetEntryAt(0, meta)->flags & ENTRY_FLAGS_RESERVED)) {
        storage = AtomicDict_GetOrCreateAccessorStorage(self);
        if (storage == NULL)
            goto fail;

        // mark entry 0 as reserved and put the remaining entries
        // into this thread's reservation buffer
        AtomicDict_GetEntryAt(0, meta)->flags |= ENTRY_FLAGS_RESERVED;
        for (i = 1; i < self->reservation_buffer_size; ++i) {
            entry_loc.entry = AtomicDict_GetEntryAt(i, meta);
            entry_loc.location = i;
            if (entry_loc.entry->key == NULL) {
                int found = 0;
                for (int j = 0; j < RESERVATION_BUFFER_SIZE; ++j) {
                    if (storage->reservation_buffer.reservations[j].location == entry_loc.location) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    AtomicDict_ReservationBufferPut(&storage->reservation_buffer, &entry_loc, 1);
                }
            }
        }
    }

    Py_DECREF(meta); // so that the only meta's refcount depends only on AtomicRef
    assert(Py_REFCNT(meta) == 1);
    return 0;
    fail:
    Py_XDECREF(meta);
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_RuntimeError, "error during initialization.");
    }
    return -1;
}

int
AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg)
{
    Py_VISIT(self->metadata);
    Py_VISIT(self->accessors);
    return 0;
}

int
AtomicDict_clear(AtomicDict *self)
{
    Py_CLEAR(self->metadata);
    Py_CLEAR(self->accessors);
    // this should be enough to deallocate the reservation buffers themselves as well:
    // the list should be the only reference to them

    if (self->accessor_key != NULL) {
        PyThread_tss_delete(self->accessor_key);
        PyThread_tss_free(self->accessor_key);
        self->accessor_key = NULL;
    }

    return 0;
}

void
AtomicDict_dealloc(AtomicDict *self)
{
    PyObject_GC_UnTrack(self);
    AtomicDict_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
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
AtomicDict_UnsafeInsert(AtomicDict_Meta *meta, Py_hash_t hash, uint64_t pos)
{
    // pos === node_index
    AtomicDict_Node temp;
    AtomicDict_Node node = {
        .index = pos,
        .tag = hash,
    };
    uint64_t ix = AtomicDict_Distance0Of(hash, meta);

    for (int probe = 0; probe < meta->max_distance; probe++) {
        AtomicDict_ReadNodeAt((ix + probe) % meta->size, &temp, meta);

        if (temp.node == 0) {
            node.distance = probe;
            AtomicDict_WriteNodeAt((ix + probe) % meta->size, &node, meta);
            goto done;
        }

        if (temp.distance < probe) {
            // non-atomic robin hood
            node.distance = probe;
            AtomicDict_WriteNodeAt((ix + probe) % meta->size, &node, meta);
            ix = ix + probe - temp.distance;
            probe = temp.distance;
            node = temp;
        }
    }
    // probes exhausted
    return -1;
    done:
    return 0;
}

int
AtomicDict_CountKeysInBlock(int64_t block_ix, AtomicDict_Meta *meta)
{
    int found = 0;

    AtomicDict_Entry *entry_p, entry;
    AtomicDict_SearchResult sr;

    for (int64_t i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
        uint64_t entry_ix = block_ix * ATOMIC_DICT_ENTRIES_IN_BLOCK + i;
        entry_p = AtomicDict_GetEntryAt(entry_ix, meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.value != NULL) {
            AtomicDict_LookupEntry(meta, entry_ix, entry.hash, &sr);
            if (sr.found) {
                found++;
            }
        }
    }

    return found;
}

PyObject *
AtomicDict_LenBounds(AtomicDict *self)
{
    AtomicDict_Meta *meta = NULL;
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int64_t gab = meta->greatest_allocated_block + 1;
    int64_t gdb = meta->greatest_deleted_block + 1;
    int64_t grb = meta->greatest_refilled_block + 1;

    int64_t supposedly_full_blocks = (gab - gdb + grb - 1);

    // visit the gab
    int64_t found = AtomicDict_CountKeysInBlock(gab - 1, meta);

    if (gab - 1 != gdb) {
        supposedly_full_blocks--;

        // visit the gdb
        found += AtomicDict_CountKeysInBlock(gdb, meta);
    }

    if (grb != gab - 1 && grb != gdb) {
        supposedly_full_blocks--;

        // visit the grb
        found += AtomicDict_CountKeysInBlock(grb, meta);
    }
    Py_DECREF(meta);
    meta = NULL;

    if (supposedly_full_blocks < 0) {
        supposedly_full_blocks = 0;
    }

    int64_t upper = supposedly_full_blocks * ATOMIC_DICT_ENTRIES_IN_BLOCK;

    Py_ssize_t threads_count = PyList_Size(self->accessors);
    int64_t lower =
        supposedly_full_blocks * ATOMIC_DICT_ENTRIES_IN_BLOCK - threads_count * self->reservation_buffer_size;

    AtomicDict_ReservationBuffer *rb;
    for (int i = 0; i < threads_count; ++i) {
        rb = &(
            (AtomicDict_AccessorStorage *) PyList_GetItemRef(self->accessors, i)
        )->reservation_buffer;

        if (rb == NULL)
            goto fail;

        lower += self->reservation_buffer_size - rb->count;
    }

    if (upper < 0) upper = 0;
    if (lower < 0) lower = 0;

    return Py_BuildValue("(ll)", lower + found, upper + found);

    fail:
    Py_XDECREF(meta);
    return NULL;
}

PyObject *
AtomicDict_ApproxLen(AtomicDict *self)
{
    PyObject *bounds = NULL;
    PyObject *lower = NULL;
    PyObject *upper = NULL;
    PyObject *sum = NULL;
    PyObject *avg = NULL;

    bounds = AtomicDict_LenBounds(self);
    if (bounds == NULL)
        goto fail;

    lower = PyTuple_GetItem(bounds, 0);
    upper = PyTuple_GetItem(bounds, 1);
    if (lower == NULL || upper == NULL)
        goto fail;

    sum = PyNumber_Add(lower, upper);
    if (sum == NULL)
        goto fail;

    avg = PyNumber_FloorDivide(sum, PyLong_FromLong(2));
    // PyLong_FromLong(2) will not return NULL

    Py_DECREF(bounds);
    Py_DECREF(lower);
    Py_DECREF(upper);
    Py_DECREF(sum);
    return avg;

    fail:
    Py_XDECREF(bounds);
    Py_XDECREF(lower);
    Py_XDECREF(upper);
    Py_XDECREF(sum);
    Py_XDECREF(avg);
    return NULL;
}

Py_ssize_t
AtomicDict_Len_impl(AtomicDict *self)
{
    PyObject *len = NULL, *local_len = NULL;
    Py_ssize_t int_len;

    if (!self->len_dirty) {
        int_len = self->len;
        return int_len;
    }

    len = PyLong_FromSsize_t(self->len);
    if (len == NULL)
        goto fail;

    for (Py_ssize_t i = 0; i < PyList_Size(self->accessors); ++i) {
        AtomicDict_AccessorStorage *storage = NULL;
        storage = (AtomicDict_AccessorStorage *) PyList_GetItemRef(self->accessors, i);
        assert(storage != NULL);

        local_len = PyLong_FromLong(storage->local_len);
        if (local_len == NULL)
            goto fail;

        len = PyNumber_InPlaceAdd(len, local_len);
        Py_DECREF(local_len);
        local_len = NULL;
        storage->local_len = 0;
    }

    int_len = PyLong_AsSsize_t(len);
    assert(!PyErr_Occurred());

    self->len = int_len;
    self->len_dirty = 0;

    Py_DECREF(len);
    return int_len;
    fail:
    Py_XDECREF(len);
    return -1;
}

Py_ssize_t
AtomicDict_Len(AtomicDict *self)
{
    Py_ssize_t len;
    AtomicDict_BeginSynchronousOperation(self);
    len = AtomicDict_Len_impl(self);
    AtomicDict_EndSynchronousOperation(self);

    return len;
}

PyObject *
AtomicDict_Debug(AtomicDict *self)
{
    AtomicDict_Meta *meta = NULL;
    PyObject *metadata = NULL;
    PyObject *index_nodes = NULL;
    PyObject *blocks = NULL;
    PyObject *entries = NULL;
    PyObject *entry_tuple = NULL;
    PyObject *block_info = NULL;

    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    metadata = Py_BuildValue("{sOsOsOsOsOsOsOsOsOsOsOsOsOsOsO}",
                             "log_size\0", Py_BuildValue("B", meta->log_size),
                             "generation\0", Py_BuildValue("n", (Py_ssize_t) meta->generation),
                             "node_size\0", Py_BuildValue("B", meta->node_size),
                             "distance_size\0", Py_BuildValue("B", meta->distance_size),
                             "tag_size\0", Py_BuildValue("B", meta->tag_size),
                             "node_mask\0", Py_BuildValue("k", meta->node_mask),
                             "index_mask\0", Py_BuildValue("k", meta->index_mask),
                             "distance_mask\0", Py_BuildValue("k", meta->distance_mask),
                             "tag_mask\0", Py_BuildValue("k", meta->tag_mask),
                             "tombstone\0", Py_BuildValue("k", meta->tombstone.node),
                             "is_compact\0", Py_BuildValue("B", meta->is_compact),
                             "inserting_block\0", Py_BuildValue("l", meta->inserting_block),
                             "greatest_allocated_block\0", Py_BuildValue("l", meta->greatest_allocated_block),
                             "greatest_deleted_block\0", Py_BuildValue("l", meta->greatest_deleted_block),
                             "greatest_refilled_block\0", Py_BuildValue("l", meta->greatest_refilled_block));
    if (metadata == NULL)
        goto fail;

    index_nodes = Py_BuildValue("[]");
    if (index_nodes == NULL)
        goto fail;

    AtomicDict_Node node;
    for (uint64_t i = 0; i < meta->size; i++) {
        AtomicDict_ReadNodeAt(i, &node, meta);
        PyObject *n = Py_BuildValue("k", node.node);
        if (n == NULL)
            goto fail;
        PyList_Append(index_nodes, n);
        Py_DECREF(n);
    }

    blocks = Py_BuildValue("[]");
    if (blocks == NULL)
        goto fail;

    AtomicDict_Block *block;
    entries = NULL;
    entry_tuple = NULL;
    block_info = NULL;
    for (uint64_t i = 0; i <= meta->greatest_allocated_block; i++) {
        block = meta->blocks[i];
        entries = Py_BuildValue("[]");
        if (entries == NULL)
            goto fail;

        for (int j = 0; j < 64; j++) {
            PyObject *key = block->entries[j].key;
            PyObject *value = block->entries[j].value;
            if (key != NULL) {
                if (value == NULL) {
                    value = PyExc_KeyError;
                }
                uint64_t entry_ix = (i << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) + j;
                entry_tuple = Py_BuildValue("(kBlOO)",
                                            entry_ix,
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

        block_info = Py_BuildValue("{snsO}",
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
    Py_DECREF(meta);
    Py_DECREF(metadata);
    Py_DECREF(blocks);
    Py_DECREF(index_nodes);
    return out;

    fail:
    PyErr_SetString(PyExc_RuntimeError, "unable to get debug info");
    Py_XDECREF(meta);
    Py_XDECREF(metadata);
    Py_XDECREF(index_nodes);
    Py_XDECREF(blocks);
    Py_XDECREF(entries);
    Py_XDECREF(entry_tuple);
    Py_XDECREF(block_info);
    return NULL;
}
