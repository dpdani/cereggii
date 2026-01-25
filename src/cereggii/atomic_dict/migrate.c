// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "constants.h"
#include "thread_id.h"
#include <stdatomic.h>


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
AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *current_meta, PyMutex *self_mutex)
{
    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
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

    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
        uintptr_t expected = 0;
        int i_am_leader = atomic_compare_exchange_strong_explicit((_Atomic(uintptr_t) *)
            &current_meta->migration_leader,
            &expected, _Py_ThreadId(), memory_order_acq_rel, memory_order_acquire);
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

    for (int64_t block_i = 0; block_i <= new_meta->greatest_allocated_block; ++block_i) {
        Py_INCREF(new_meta->blocks[block_i]);
    }


    int32_t accessors_len = atomic_load_explicit((_Atomic (int32_t) *) &self->accessors_len, memory_order_acquire);
    assert(accessors_len > 0);
    int64_t *participants = PyMem_RawMalloc(sizeof(int64_t) * accessors_len);
    if (participants == NULL) {
        PyErr_NoMemory();
        goto fail;
    }
    for (int32_t i = 0; i < accessors_len; ++i) {
        atomic_store_explicit((_Atomic (int64_t) *) &participants[i], 0, memory_order_release);
    }
    atomic_store_explicit((_Atomic (int64_t *) *) &current_meta->participants, participants, memory_order_release);
    atomic_store_explicit((_Atomic (int32_t) *) &current_meta->participants_count, accessors_len, memory_order_release);

    int64_t inserted_before_migration = 0;
    int64_t tombstones_before_migration = 0;
    AtomicDict_AccessorStorage *accessor;
    FOR_EACH_ACCESSOR(self, accessor) {
        int64_t local_inserted = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
        int64_t local_tombstones = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, memory_order_acquire);
        inserted_before_migration += local_inserted;
        tombstones_before_migration += local_tombstones;
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_inserted, 0, memory_order_release);
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, 0, memory_order_release);
    }
    cereggii_unused_in_release_build(inserted_before_migration);
    cereggii_unused_in_release_build(tombstones_before_migration);

    if (from_log_size < to_log_size) {
        atomic_store_explicit((_Atomic (Py_tss_t *) *) &current_meta->accessor_key, self->accessor_key, memory_order_release);

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
    atomic_store_explicit((_Atomic (AtomicDict_Meta *) *) &current_meta->new_gen_metadata, new_meta, memory_order_release);
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);
    cereggii_unused_in_release_build(set);

    if (from_log_size < to_log_size) {
        assert(holding_sync_lock);
        int64_t inserted_after_migration = 0;
        FOR_EACH_ACCESSOR(self, accessor) {
            inserted_after_migration += atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
        }
        assert(inserted_after_migration == inserted_before_migration - tombstones_before_migration);
        cereggii_unused_in_release_build(inserted_after_migration);
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
    AtomicDict_Meta *new_meta = atomic_load_explicit((_Atomic (AtomicDict_Meta *) *) &current_meta->new_gen_metadata, memory_order_acquire);

    AtomicDict_CommonMigrate(current_meta, new_meta);

    AtomicEvent_Wait(current_meta->migration_done);
}

void
AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    if (AtomicEvent_IsSet(current_meta->node_migration_done))
        return;

    Py_tss_t *ak = atomic_load_explicit((_Atomic(Py_tss_t *) *) &current_meta->accessor_key, memory_order_acquire);
    AtomicDict_AccessorStorage *storage = AtomicDict_GetAccessorStorage(ak);
    assert(storage != NULL);

    // _Atomic(int64_t) *participant = (_Atomic(int64_t) *) &current_meta->participants[storage->accessor_ix];
    int64_t *participants = atomic_load_explicit((_Atomic(int64_t *) *) &current_meta->participants, memory_order_acquire);
    int64_t *participant = &participants[storage->accessor_ix];
    int64_t expected, ok;

    expected = 0ll;
    ok = atomic_compare_exchange_strong_explicit((_Atomic (int64_t) *) participant, &expected, 1ll, memory_order_acq_rel, memory_order_acquire);
    assert(ok);
    cereggii_unused_in_release_build(ok);

    int64_t migrated_count = AtomicDict_MigrateNodes(current_meta, new_meta);

    expected = 1ll;
    ok = atomic_compare_exchange_strong_explicit((_Atomic (int64_t) *) participant, &expected, 2ll, memory_order_acq_rel, memory_order_acquire);
    assert(ok);
    cereggii_unused_in_release_build(ok);
    atomic_store_explicit((_Atomic(int64_t) *) &storage->local_inserted, migrated_count, memory_order_release);

    if (AtomicDict_NodesMigrationDone(current_meta)) {
        AtomicEvent_Set(current_meta->node_migration_done);
    }
    AtomicEvent_Wait(current_meta->node_migration_done);
}

int
AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t thread_id = _Py_ThreadId();

    int64_t copy_lock;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        uint64_t lock = (copy_lock + thread_id) % (new_meta->greatest_allocated_block + 1);

        void *expected = current_meta->generation;
        int locked = atomic_compare_exchange_strong_explicit((_Atomic(void *) *) &new_meta->blocks[lock]->generation,
                    &expected, NULL, memory_order_acq_rel, memory_order_acquire);
        if (!locked)
            continue;

        if ((uint64_t) new_meta->greatest_refilled_block < lock && lock <= (uint64_t) new_meta->greatest_deleted_block)
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
            cereggii_unused_in_release_build(result);
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

inline void
AtomicDict_MigrateNode(AtomicDict_Node *node, AtomicDict_Meta *new_meta, const uint64_t trailing_cluster_start, const uint64_t trailing_cluster_size)
{
    assert(node->index != 0);
    Py_hash_t hash = AtomicDict_GetEntryAt(node->index, new_meta)->hash;
    uint64_t d0 = AtomicDict_Distance0Of(hash, new_meta);
    node->tag = hash;
    uint64_t position;

    for (int64_t distance = 0; distance < SIZE_OF(new_meta); distance++) {
        position = (d0 + distance) & (SIZE_OF(new_meta) - 1);

        if (AtomicDict_ReadRawNodeAt(position, new_meta) == 0) {
            uint64_t range_start = (trailing_cluster_start * 2) & (SIZE_OF(new_meta) - 1);
            uint64_t range_end = (2 * (trailing_cluster_start + trailing_cluster_size + 1)) & (SIZE_OF(new_meta) - 1);
            if (range_start < range_end) {
                assert(position >= range_start && position < range_end);
            } else {
                assert(position >= range_start || position < range_end);
            }
            AtomicDict_WriteNodeAt(position, node, new_meta);
            break;
        }
    }
}

void
initialize_in_new_meta(AtomicDict_Meta *new_meta, const uint64_t start, const uint64_t end)
{
    // initialize slots in range [start, end)
    for (uint64_t j = 2 * start; j < 2 * (end + 1); ++j) {
        AtomicDict_WriteRawNodeAt(j & (SIZE_OF(new_meta) - 1), 0, new_meta);
    }
}

#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 4096


int64_t
to_migrate(AtomicDict_Meta *current_meta, int64_t start_of_block, uint64_t end_of_block)
{
    uint64_t current_size = SIZE_OF(current_meta);
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;
    int64_t to_migrate = 0;
    AtomicDict_Node node = {0};

    while (i < end_of_block) {
        if (AtomicDict_ReadRawNodeAt(i, current_meta) == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return 0;

    while (i < end_of_block) {
        AtomicDict_ReadNodeAt(i, &node, current_meta);
        if (node.node != 0 && node.index != 0) {
            to_migrate++;
        }
        i++;
    }

    assert(i == end_of_block);

    do {
        AtomicDict_ReadNodeAt(i & current_size_mask, &node, current_meta);
        if (node.node != 0 && node.index != 0) {
            to_migrate++;
        }
        i++;
    } while (node.node != 0);

    return to_migrate;
}

int64_t
AtomicDict_BlockWiseMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta, int64_t start_of_block)
{
    int64_t migrated_count = 0;
    uint64_t current_size = SIZE_OF(current_meta);
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;

    uint64_t end_of_block = start_of_block + ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE;
    if (end_of_block > current_size) {
        end_of_block = current_size;
    }
    assert(end_of_block > i);

    AtomicDict_Node node = {0};

    // find first empty slot
    while (i < end_of_block) {
        if (AtomicDict_ReadRawNodeAt(i, current_meta) == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return 0;

    uint64_t start_of_cluster = i;
    uint64_t cluster_size = 0;

    initialize_in_new_meta(new_meta, i, end_of_block);

    for (; i < end_of_block; i++) {
        AtomicDict_ReadNodeAt(i, &node, current_meta);

        if (node.node == 0) {
            start_of_cluster = i + 1;
            cluster_size = 0;
            continue;
        }
        cluster_size++;
        if (node.index == 0 /*tombstone*/)
            continue;

        AtomicDict_MigrateNode(&node, new_meta, start_of_cluster, cluster_size);
        migrated_count++;
    }

    assert(i == end_of_block); // that is, start of next block

    if (cluster_size == 0) {
        start_of_cluster = end_of_block & current_size_mask;
    }

    uint64_t j = end_of_block;
    while (1) {
        AtomicDict_ReadNodeAt(j & current_size_mask, &node, current_meta);
        if (node.node == 0) {
            break;
        }
        j++;
    }
    if (j > end_of_block) {
        initialize_in_new_meta(new_meta, end_of_block, j - 1);
        while (1) {
            AtomicDict_ReadNodeAt(i & current_size_mask, &node, current_meta);
            if (node.node == 0) {
                break;
            }
            cluster_size++;
            if (node.index != 0 /*tombstone*/) {
                AtomicDict_MigrateNode(&node, new_meta, start_of_cluster, cluster_size);
                migrated_count++;
            }
            i++;
        }
    }

    assert(to_migrate(current_meta, start_of_block, end_of_block) == migrated_count);
    return migrated_count;
}

int64_t
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t current_size = SIZE_OF(current_meta);
    int64_t node_to_migrate = atomic_fetch_add_explicit((_Atomic(int64_t) *) &current_meta->node_to_migrate,
                                                      ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE, memory_order_acq_rel);
    int64_t migrated_count = 0;

    while ((uint64_t) node_to_migrate < current_size) {
        migrated_count += AtomicDict_BlockWiseMigrate(current_meta, new_meta, node_to_migrate);
        node_to_migrate = atomic_fetch_add_explicit((_Atomic(int64_t) *) &current_meta->node_to_migrate, ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE, memory_order_acq_rel);
    }

    return migrated_count;
}

int
AtomicDict_NodesMigrationDone(AtomicDict_Meta *current_meta)
{
    int done = 1;

    int64_t *participants = atomic_load_explicit((_Atomic(int64_t *) *) &current_meta->participants, memory_order_acquire);

    for (int32_t accessor = 0; accessor < current_meta->participants_count; accessor++) {
        if (atomic_load_explicit((_Atomic(int64_t) *) &participants[accessor], memory_order_acquire) == 1ll) {
            done = 0;
            break;
        }
    }

    return done;
}
