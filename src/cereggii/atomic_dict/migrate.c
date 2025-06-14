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
    AtomicDict_AccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    meta = AtomicDict_GetMeta(self, storage);

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size + 1);
    if (migrate < 0)
        goto fail;

    return migrate;

    fail:
    return -1;
}

int
AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *current_meta, PyMutex *self_mutex, AtomicDict_AccessorStorage *accessors)
{
    if (current_meta->migration_leader == 0) {
        return 0;
    }

    PyMutex_Unlock(self_mutex);
    AtomicDict_FollowerMigrate(current_meta, accessors);
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

    AtomicDict_FollowerMigrate(current_meta, self->accessors);

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
    AtomicDict_CommonMigrate(current_meta, new_meta, self->accessors);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);

    if (from_log_size < to_log_size) {
        assert(holding_sync_lock);
        for (AtomicDict_AccessorStorage *accessor = self->accessors; accessor != NULL; accessor = accessor->next_accessor) {
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
AtomicDict_FollowerMigrate(AtomicDict_Meta *current_meta, AtomicDict_AccessorStorage *accessors)
{
    AtomicEvent_Wait(current_meta->new_metadata_ready);
    AtomicDict_Meta *new_meta = current_meta->new_gen_metadata;

    AtomicDict_CommonMigrate(current_meta, new_meta, accessors);

    AtomicEvent_Wait(current_meta->migration_done);
}

void
AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta, AtomicDict_AccessorStorage *accessors)
{
    if (!AtomicEvent_IsSet(current_meta->node_migration_done)) {
        // assert(self->accessors_lock is held by leader);
        AtomicDict_AccessorStorage *storage = AtomicDict_GetAccessorStorage(current_meta->accessor_key);
        CereggiiAtomic_StoreInt(&storage->participant_in_migration, 1);

        AtomicDict_MigrateNodes(current_meta, new_meta);

        if (AtomicDict_NodesMigrationDone(accessors)) {
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

            if (entry_loc.entry->key == NULL || entry_loc.entry->value == NULL)
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

    for (uint64_t i = 0; i < SIZE_OF(meta); ++i) {
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

    for (uint64_t i = 0; i < SIZE_OF(meta); ++i) {
        AtomicDict_ReadNodeAt(i, &node, meta);

        if (node.node != 0 && node.node != TOMBSTONE(meta))
            len++;
    }

    return len;
}

inline void
AtomicDict_MigrateNode(AtomicDict_Node *node, AtomicDict_Meta *new_meta)
{
    // assert(AtomicDict_IndexNotFound(node->index, new_meta));
    Py_hash_t hash = AtomicDict_GetEntryAt(node->index, new_meta)->hash;
    uint64_t d0 = AtomicDict_Distance0Of(hash, new_meta);
    node->tag = hash;
    uint64_t position;

    for (uint64_t distance = 0; distance < SIZE_OF(new_meta); distance++) {
        position = (d0 + distance) & (SIZE_OF(new_meta) - 1);

        if (new_meta->index[position] == 0) {
            AtomicDict_WriteNodeAt(position, node, new_meta);
            break;
        }
    }
}

#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 4096

void
AtomicDict_BlockWiseMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta, int64_t start_of_block)
{
    uint64_t current_size = SIZE_OF(current_meta);
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
        if (current_meta->index[i] == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return;

    // initialize slots in range [i, end_of_block]
    memset(&new_meta->index[i * 2], 0, (end_of_block - i) * 2 * sizeof(uint64_t));

    uint64_t new_size_mask = SIZE_OF(new_meta) - 1;

    for (; i < end_of_block; i++) {
        AtomicDict_ReadNodeAt(i, &node, current_meta);

        if (node.node == 0 || node.node == TOMBSTONE(new_meta))
            continue;

        AtomicDict_MigrateNode(&node, new_meta);
    }

    assert(i == end_of_block);

    while (node.node != 0 || i == end_of_block) {
        memset(&new_meta->index[(i * 2) & new_size_mask], 0, ((i + 1) * 2 - i * 2) * sizeof(uint64_t));
        AtomicDict_ReadNodeAt(i & current_size_mask, &node, current_meta);

        if (node.node != 0 && node.node != TOMBSTONE(current_meta)) {
            AtomicDict_MigrateNode(&node, new_meta);
        }

        i++;
    }
}

void
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t current_size = SIZE_OF(current_meta);

    int64_t node_to_migrate = CereggiiAtomic_AddInt64(&current_meta->node_to_migrate,
                                                      ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE);

    while (node_to_migrate < current_size) {
        AtomicDict_BlockWiseMigrate(current_meta, new_meta, node_to_migrate);
        node_to_migrate = CereggiiAtomic_AddInt64(&current_meta->node_to_migrate, ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE);
    }

    AtomicDict_AccessorStorage *storage = AtomicDict_GetAccessorStorage(current_meta->accessor_key);
    CereggiiAtomic_StoreInt(&storage->participant_in_migration, 2);
}

int
AtomicDict_NodesMigrationDone(AtomicDict_AccessorStorage *accessors)
{
    int done = 1;

    for (AtomicDict_AccessorStorage *storage = accessors; storage != NULL; storage = storage->next_accessor) {
        if (storage->participant_in_migration == 1) {
            done = 0;
            break;
        }
    }

    return done;
}
