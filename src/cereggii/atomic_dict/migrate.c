// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "constants.h"


int
AtomicDict_Grow(AtomicDict *self)
{
    AtomicDict_Meta *meta = NULL;
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size + 1);
    if (migrate < 0)
        goto fail;

    Py_DECREF(meta);
    return migrate;

    fail:
    Py_XDECREF(meta);
    return -1;
}

int
AtomicDict_Shrink(AtomicDict *self)
{
    AtomicDict_Meta *meta = NULL;
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    if (meta->log_size == self->min_log_size) {
        Py_DECREF(meta);
        return 0;
    }

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size - 1);
    if (migrate < 0)
        goto fail;

    Py_DECREF(meta);
    return migrate;

    fail:
    Py_XDECREF(meta);
    return -1;
}

int
AtomicDict_Compact(AtomicDict *self)
{
    int migrate, is_compact = 1;
    AtomicDict_Meta *meta = NULL;

    do {
        /**
         * do one migration with equal log_sizes first to reduce the blocks.
         * then, if the resulting meta is not compact, increase the log_size
         * and migrate, until meta is compact.
         */

        meta = NULL;
        meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
        if (meta == NULL)
            goto fail;

        migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size + !is_compact);
        if (migrate < 0)
            goto fail;

        Py_DECREF(meta);
        meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
        is_compact = meta->is_compact;
        Py_DECREF(meta);

    } while (!is_compact);

    return migrate;

    fail:
    Py_XDECREF(meta);
    return -1;
}

PyObject *
AtomicDict_Compact_callable(AtomicDict *self)
{
    int migrate = AtomicDict_Compact(self);

    if (migrate < 0) {
        PyErr_SetString(PyExc_RuntimeError, "error during compaction.");
        return NULL;
    }

    Py_RETURN_NONE;
}


int
AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *current_meta, PyMutex *self_mutex)
{
    if (current_meta->migration_leader == 0) {
        return 0;
    }

    _PyMutex_unlock(self_mutex);
    AtomicDict_FollowerMigrate(current_meta);
    return 1;
}


int
AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta /* borrowed */, uint8_t from_log_size,
                   uint8_t to_log_size)
{
    assert(to_log_size <= from_log_size + 1);
    assert(to_log_size >= from_log_size - 1);

    if (current_meta->migration_leader == 0) {
        int i_am_leader = CereggiiAtomic_CompareExchangeUIntPtr(
            &current_meta->migration_leader,
            0, _Py_ThreadId());
        if (i_am_leader) {
            return AtomicDict_LeaderMigrate(self, current_meta, from_log_size, to_log_size);
        }
    }

    AtomicDict_FollowerMigrate(current_meta);

    return 1;
}

int
AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDict_Meta *current_meta /* borrowed */, uint8_t from_log_size,
                         uint8_t to_log_size)
{
    int holding_sync_lock = 0;
    AtomicDict_Meta *new_meta;

    beginning:
    new_meta = NULL;
    new_meta = AtomicDictMeta_New(to_log_size);
    if (new_meta == NULL)
        goto fail;

    if (to_log_size > ATOMIC_DICT_MAX_LOG_SIZE) {
        PyErr_SetString(PyExc_ValueError, "can hold at most 2^56 items.");
        goto fail;
    }

    if (to_log_size < self->min_log_size) {
        to_log_size = self->min_log_size;
        Py_DECREF(new_meta);
        new_meta = NULL;
        goto beginning;
    }

    // blocks
    AtomicDict_BeginSynchronousOperation(self);
    holding_sync_lock = 1;
    if (from_log_size < to_log_size) {
        int ok = AtomicDictMeta_CopyBlocks(current_meta, new_meta);

        if (ok < 0)
            goto fail;
    } else {
        int ok = AtomicDictMeta_InitBlocks(new_meta);

        if (ok < 0)
            goto fail;

        AtomicDictMeta_ShrinkBlocks(self, current_meta, new_meta);
    }

    for (uint64_t block_i = 0; block_i <= new_meta->greatest_allocated_block; ++block_i) {
        Py_INCREF(new_meta->blocks[block_i]);
    }

    AtomicDictMeta_ClearIndex(new_meta);

    if (from_log_size >= to_log_size) {
        AtomicDict_EndSynchronousOperation(self);
        holding_sync_lock = 0;
    } else {
        current_meta->hashes = PyMem_RawMalloc(current_meta->size * sizeof(Py_hash_t));
        if (current_meta->hashes == NULL)
            goto fail;

        current_meta->accessor_key = self->accessor_key;
        current_meta->accessors = self->accessors;

        for (int64_t block = 0; block <= new_meta->greatest_allocated_block; ++block) {
            new_meta->blocks[block]->generation = new_meta->generation;
        }
    }

    // 👀
    Py_INCREF(new_meta);
    current_meta->new_gen_metadata = new_meta;
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);

    if (holding_sync_lock) {
        for (int i = 0; i < PyList_Size(self->accessors); ++i) {
            AtomicDict_AccessorStorage *accessor = (AtomicDict_AccessorStorage *) PyList_GetItem(self->accessors, i);
            accessor->participant_in_migration = 0;
        }
    }

    AtomicEvent_Set(current_meta->migration_done);
    Py_DECREF(new_meta);  // this may seem strange: why decref the new meta?
    // the reason is that AtomicRef_CompareAndSet also increases new_meta's refcount,
    // which is exactly what we want. but the reference count was already 1, as it
    // was set during the initialization of new_meta. that's what we're decref'ing
    // for in here.

    if (holding_sync_lock) {
        AtomicDict_EndSynchronousOperation(self);
    }
    return 1;

    fail:
    if (holding_sync_lock) {
        AtomicDict_EndSynchronousOperation(self);
    }
    // don't block other threads indefinitely
    AtomicEvent_Set(current_meta->migration_done);
    AtomicEvent_Set(current_meta->hashes_done);
    AtomicEvent_Set(current_meta->node_migration_done);
    AtomicEvent_Set(current_meta->new_metadata_ready);
    return -1;
}

void
AtomicDict_FollowerMigrate(AtomicDict_Meta *current_meta)
{
    AtomicEvent_Wait(current_meta->new_metadata_ready);
    AtomicDict_Meta *new_meta = current_meta->new_gen_metadata;

    AtomicDict_CommonMigrate(current_meta, new_meta);

    AtomicEvent_Wait(current_meta->migration_done);
}

void
AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    if (current_meta->log_size < new_meta->log_size) {
        AtomicDict_QuickMigrate(current_meta, new_meta);
    } else {
        AtomicDict_SlowMigrate(current_meta, new_meta);
    }
}

void
AtomicDict_SlowMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    if (!AtomicEvent_IsSet(current_meta->node_migration_done)) {
        int node_migration_done = AtomicDict_MigrateReInsertAll(current_meta, new_meta);

        if (node_migration_done) {
            AtomicEvent_Set(current_meta->node_migration_done);
        }
        AtomicEvent_Wait(current_meta->node_migration_done);
    }
}

void
AtomicDict_QuickMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
//    if (!AtomicEvent_IsSet(current_meta->hashes_done)) {
//        int hashes_done = AtomicDict_PrepareHashArray(current_meta, new_meta);
//
//        if (hashes_done) {
//            AtomicEvent_Set(current_meta->hashes_done);
//        }
//        AtomicEvent_Wait(current_meta->hashes_done);
//    }
    if (!AtomicEvent_IsSet(current_meta->node_migration_done)) {
        // assert(self->accessors_lock is held by leader);
        AtomicDict_AccessorStorage *storage = AtomicDict_GetAccessorStorage(current_meta->accessor_key);
        CereggiiAtomic_StoreInt(&storage->participant_in_migration, 1);

        int node_migration_done = AtomicDict_MigrateNodes(current_meta, new_meta);

        if (node_migration_done) {
            AtomicEvent_Set(current_meta->node_migration_done);
        }
        AtomicEvent_Wait(current_meta->node_migration_done);
    }
}

int
AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t thread_id = _Py_ThreadId();

    int64_t copy_lock;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        uint64_t lock = (copy_lock + thread_id) % (new_meta->greatest_allocated_block + 1);

        int locked = CereggiiAtomic_CompareExchangePtr((void **) &new_meta->blocks[lock]->generation,
                                                       current_meta->generation, NULL);
        if (!locked)
            continue;

        if (new_meta->greatest_refilled_block < lock && lock <= new_meta->greatest_deleted_block)
            goto mark_as_done;

        AtomicDict_EntryLoc entry_loc;

        for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
            entry_loc.location = (lock << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) + i;
            entry_loc.entry = AtomicDict_GetEntryAt(entry_loc.location, new_meta);

            if (entry_loc.entry->key == NULL || entry_loc.entry->value == NULL ||
                entry_loc.entry->flags & ENTRY_FLAGS_TOMBSTONE || entry_loc.entry->flags & ENTRY_FLAGS_SWAPPED)
                continue;

            AtomicDict_SearchResult sr;
            AtomicDict_Lookup(current_meta, entry_loc.entry->key, entry_loc.entry->hash, &sr);

            if (sr.found) {
                int must_grow;
                PyObject *result = AtomicDict_ExpectedInsertOrUpdate(new_meta,
                                                                     entry_loc.entry->key, entry_loc.entry->hash,
                                                                     NOT_FOUND, entry_loc.entry->value,
                                                                     &entry_loc, &must_grow, 1);
                assert(result != EXPECTATION_FAILED);
                assert(!must_grow);
                assert(result != NULL);
                assert(result == NOT_FOUND);
            }
        }

        mark_as_done:
        new_meta->blocks[lock]->generation = new_meta->generation; // mark as done
    }

    int done = 1;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        if (new_meta->blocks[copy_lock]->generation != new_meta->generation) {
            done = 0;
            break;
        }
    }

    return done;
}

int
AtomicDict_PrepareHashArray(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t thread_id = _Py_ThreadId();

    int64_t block_i;
    AtomicDict_Block *block;

    for (block_i = 0; block_i <= new_meta->greatest_allocated_block; ++block_i) {
        uint64_t lock = (block_i + thread_id) % (new_meta->greatest_allocated_block + 1);

        block = new_meta->blocks[lock];
        if ((void **) block == (void **) NOT_FOUND)
            continue;

        CereggiiAtomic_StorePtr((void **) &new_meta->blocks[lock], NOT_FOUND);  // soft-locking

        if (new_meta->greatest_refilled_block < lock && lock <= new_meta->greatest_deleted_block)
            goto mark_as_done;

        AtomicDict_EntryLoc entry_loc;

        for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
            entry_loc.location = (lock << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) + i;
            entry_loc.entry = &block->entries[i];

            if (entry_loc.entry->key == NULL || entry_loc.entry->value == NULL ||
                entry_loc.entry->flags & ENTRY_FLAGS_TOMBSTONE || entry_loc.entry->flags & ENTRY_FLAGS_SWAPPED)
                continue;

            AtomicDict_SearchResult sr;
            AtomicDict_LookupEntry(current_meta, entry_loc.location, entry_loc.entry->hash, &sr);

            if (sr.found) {
                assert(&current_meta->hashes[sr.position] < current_meta->hashes + current_meta->size);
                current_meta->hashes[sr.position] = entry_loc.entry->hash;
            }
        }

        mark_as_done:
        block->generation = new_meta->generation;
        CereggiiAtomic_StorePtr((void **) &new_meta->blocks[lock], block);
    }

    int done = 1;

    for (block_i = 0; block_i <= new_meta->greatest_allocated_block; ++block_i) {
        AtomicDict_Block *b = new_meta->blocks[block_i];

        if ((void *) b == (void *) NOT_FOUND) {
            done = 0;
            break;
        }

        if (b->generation != new_meta->generation) {
            done = 0;
            break;
        }
    }

    return done;
}

void
AtomicDict_MigrateNode(uint64_t current_size_mask, AtomicDict_Meta *new_meta, AtomicDict_Node *node, Py_hash_t hash)
{
//    AtomicDict_SearchResult search;
//    AtomicDict_LookupEntry(new_meta, node->index, hash, &search);
//    if (search.found)
//        return;

    uint64_t ix, d0 = AtomicDict_Distance0Of(hash, new_meta);

    if (!new_meta->is_compact)
        goto non_compact;

    node->tag = hash;

    AtomicDict_RobinHoodResult inserted = AtomicDict_RobinHoodInsertRaw(new_meta, node, (int64_t) d0);

    if (inserted == grow || inserted == failed) {
        new_meta->is_compact = 0;

        non_compact:
        ix = d0;

        while (AtomicDict_ReadRawNodeAt(ix, new_meta) != 0) {
            ix++;
            ix &= current_size_mask;
        }

        node->distance = new_meta->max_distance;
        node->tag = hash;

        AtomicDict_WriteNodeAt(ix, node, new_meta);
    }
}

void
AtomicDict_SeekToProbeStart(AtomicDict_Meta *meta, uint64_t *pos, uint64_t displacement, uint64_t current_size_mask)
{
    uint64_t node;

    assert(AtomicDict_ReadRawNodeAt((displacement + *pos) & current_size_mask, meta) == 0);

    do {
        (*pos)++;
        node = AtomicDict_ReadRawNodeAt((displacement + *pos) & current_size_mask, meta);
    } while (node == 0);
}

void
AtomicDict_SeekToProbeEnd(AtomicDict_Meta *meta, uint64_t *pos, uint64_t displacement, uint64_t current_size_mask)
{
    uint64_t node;

    assert(AtomicDict_ReadRawNodeAt((displacement + *pos) & current_size_mask, meta) != 0);

    do {
        (*pos)++;
        node = AtomicDict_ReadRawNodeAt((displacement + *pos) & current_size_mask, meta);
    } while (node != 0);
}

int
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t thread_id = _Py_ThreadId();

    uint64_t current_size = current_meta->size;
    uint64_t current_size_mask = current_size - 1;
    uint64_t displacement = thread_id & current_size_mask;

    AtomicDict_Node node, taken = {
        .index = 0,
        .distance = 1,
        .tag = 0,
    };
    AtomicDict_ComputeRawNode(&taken, current_meta);

    uint64_t probe_start, probe_length;
    uint64_t distance_lx = 0, distance_rx = 0;
    uint64_t i = 0;

    AtomicDict_ReadNodeAt(displacement, &node, current_meta);

    if (node.node != 0 && node.distance != 0) { // todo: handle tombstone probe head
        AtomicDict_SeekToProbeEnd(current_meta, &i, displacement, current_size_mask);
    }
    if (AtomicDict_ReadRawNodeAt((i + displacement) & current_size_mask, current_meta) == 0) {
        AtomicDict_SeekToProbeStart(current_meta, &i, displacement, current_size_mask);
    }

    probe_start = (displacement + i) & current_size_mask;
    probe_length = 0;

    for (; i < current_size; ++i) {
        uint64_t ix = (displacement + i) & current_size_mask;

        AtomicDict_ReadNodeAt(ix, &node, current_meta);

        if (node.node == taken.node) {
            seek_to_next_probe:
            AtomicDict_SeekToProbeEnd(current_meta, &i, displacement, current_size_mask);
            AtomicDict_SeekToProbeStart(current_meta, &i, displacement, current_size_mask);
        }

        if (node.node == 0) {
            assert(distance_lx > 0 || distance_rx > 0);

            distance_lx = 0;
            distance_rx = 0;
            AtomicDict_SeekToProbeStart(current_meta, &i, displacement, current_size_mask);
            probe_start = (displacement + i) & current_size_mask;
            probe_length = 0;

            i--;
            continue;
        }

        if (ix == probe_start) {
//            AtomicDict_WriteRawNodeAt(ix, taken.node, current_meta);
            if (!AtomicDict_AtomicWriteNodesAt(ix, 1, &node, &taken, current_meta))
                goto seek_to_next_probe;
        }
        probe_length++;

        if (AtomicDict_NodeIsTombstone(&node, current_meta))
            continue;

        Py_hash_t hash = AtomicDict_GetEntryAt(node.index, new_meta)->hash;
        if (AtomicDict_Distance0Of(hash, current_meta) == AtomicDict_Distance0Of(hash, new_meta)) {
            AtomicDict_MigrateNode(current_size_mask, new_meta, &node, hash);
            distance_lx++;
        } else {
            AtomicDict_MigrateNode(current_size_mask, new_meta, &node, hash);
            distance_rx++;
        }
    }

    AtomicDict_AccessorStorage *storage = AtomicDict_GetAccessorStorage(current_meta->accessor_key);
    CereggiiAtomic_StoreInt(&storage->participant_in_migration, 2);

    return AtomicDict_NodesMigrationDone(current_meta);
}

int
AtomicDict_NodesMigrationDone(const AtomicDict_Meta *current_meta)
{
    int done = 1;

    for (Py_ssize_t migrator = 0; migrator < PyList_Size(current_meta->accessors); ++migrator) {
        AtomicDict_AccessorStorage *storage =
            (AtomicDict_AccessorStorage *) PyList_GetItem(current_meta->accessors, migrator);

        if (storage->participant_in_migration == 1) {
            done = 0;
            break;
        }
    }

    return done;
}
