// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "constants.h"
#include "thread_id.h"


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

    PyMutex_Unlock(self_mutex);
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

    if (from_log_size < to_log_size) {
        current_meta->accessor_key = self->accessor_key;
        current_meta->accessors = self->accessors;

        for (int64_t block = 0; block <= new_meta->greatest_allocated_block; ++block) {
            new_meta->blocks[block]->generation = new_meta->generation;
        }
    } else {
        AtomicDictMeta_ClearIndex(new_meta);
        AtomicDict_EndSynchronousOperation(self);
        holding_sync_lock = 0;
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

    if (from_log_size < to_log_size) {
        assert(holding_sync_lock);
        for (int i = 0; i < PyList_Size(self->accessors); ++i) {
            AtomicDict_AccessorStorage *accessor = (AtomicDict_AccessorStorage *) PyList_GetItemRef(self->accessors, i);
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

__attribute__((unused)) int
AtomicDict_IndexNotFound(uint64_t index, AtomicDict_Meta *meta)
{
    AtomicDict_Node node;

    for (uint64_t i = 0; i < meta->size; ++i) {
        AtomicDict_ReadNodeAt(i, &node, meta);

        if (node.index == index) {
            return 0;
        }
    }

    return 1;
}

__attribute__((unused)) uint64_t
AtomicDict_DebugLen(AtomicDict_Meta *meta)
{
    uint64_t len = 0;
    AtomicDict_Node node;

    for (uint64_t i = 0; i < meta->size; ++i) {
        AtomicDict_ReadNodeAt(i, &node, meta);

        if (node.node != 0 && !AtomicDict_NodeIsTombstone(&node, meta))
            len++;
    }

    return len;
}

inline void
AtomicDict_MigrateNode(AtomicDict_Node *node, uint64_t *distance_0, uint64_t *distance, AtomicDict_Meta *new_meta,
                       uint64_t size_mask)
{
    // assert(AtomicDict_IndexNotFound(node->index, new_meta));
    Py_hash_t hash = AtomicDict_GetEntryAt(node->index, new_meta)->hash;
    uint64_t ix;
    uint64_t d0 = AtomicDict_Distance0Of(hash, new_meta);

    if (d0 != *distance_0) {
        *distance_0 = d0;
        *distance = 0;
    }

    node->tag = hash;

    if (!new_meta->is_compact) {
        do {
            ix = (d0 + *distance) & size_mask;
            (*distance)++;
        } while (AtomicDict_ReadRawNodeAt(ix, new_meta) != 0);

        node->distance = new_meta->max_distance;

        assert(AtomicDict_ReadRawNodeAt(ix, new_meta) == 0);
        AtomicDict_WriteNodeAt(ix, node, new_meta);

        return;
    }

    AtomicDict_RobinHoodResult inserted = AtomicDict_RobinHoodInsertRaw(new_meta, node, (int64_t) d0);

    assert(inserted != failed);

    if (inserted == grow) {
        new_meta->is_compact = 0;
    }

    (*distance)++;
}

#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 4096
//#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 64

void
AtomicDict_BlockWiseMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta, int64_t start_of_block)
{
    uint64_t current_size = current_meta->size;
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;

    uint64_t end_of_block = start_of_block + ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE;
    if (end_of_block > current_size) {
        end_of_block = current_size;
    }
    assert(end_of_block > i);

    AtomicDict_Node node;

    // find first empty slot
    while (i < end_of_block) {
        if (AtomicDict_ReadRawNodeAt(i, current_meta) == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return;

    // initialize slots in range [i, end_of_block]
    memset(AtomicDict_IndexAddressOf(i * 2, new_meta), 0, (end_of_block - i) * 2 * new_meta->node_size / 8);

    uint64_t new_size_mask = new_meta->size - 1;
    uint64_t distance = 0;
    uint64_t distance_0 = ULLONG_MAX;

    for (; i < end_of_block; i++) {
        AtomicDict_ReadNodeAt(i, &node, current_meta);

        if (node.node == 0 || AtomicDict_NodeIsTombstone(&node, current_meta))
            continue;

        AtomicDict_MigrateNode(&node, &distance_0, &distance, new_meta, new_size_mask);
    }

    assert(i == end_of_block);

    while (node.node != 0 || i == end_of_block) {
        memset(
            AtomicDict_IndexAddressOf((i * 2) & new_size_mask, new_meta), 0,
            ((i + 1) * 2 - (i * 2)) * new_meta->node_size / 8
        );
        AtomicDict_ReadNodeAt(i & current_size_mask, &node, current_meta);

        if (node.node != 0 && !AtomicDict_NodeIsTombstone(&node, current_meta)) {
            AtomicDict_MigrateNode(&node, &distance_0, &distance, new_meta, new_size_mask);
        }

        i++;
    }
}

int
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t current_size = current_meta->size;

    int64_t node_to_migrate = CereggiiAtomic_AddInt64(&current_meta->node_to_migrate,
                                                      ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE);

    while (node_to_migrate < current_size) {
        AtomicDict_BlockWiseMigrate(current_meta, new_meta, node_to_migrate);
        node_to_migrate = CereggiiAtomic_AddInt64(&current_meta->node_to_migrate, ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE);
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
            (AtomicDict_AccessorStorage *) PyList_GetItemRef(current_meta->accessors, migrator);

        if (storage->participant_in_migration == 1) {
            done = 0;
            break;
        }
    }

    return done;
}
