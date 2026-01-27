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
    AtomicDictMeta *meta = NULL;
    AtomicDictAccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    meta = AtomicDict_GetMeta(self, storage);

    int migrate = AtomicDict_Migrate(self, meta);
    if (migrate < 0)
        goto fail;

    return migrate;

    fail:
    return -1;
}

int
AtomicDict_MaybeHelpMigrate(AtomicDictMeta *current_meta, PyMutex *self_mutex)
{
    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
        return 0;
    }

    PyMutex_Unlock(self_mutex);
    AtomicDict_FollowerMigrate(current_meta);
    return 1;
}


int
AtomicDict_Migrate(AtomicDict *self, AtomicDictMeta *current_meta /* borrowed */)
{
    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
        uintptr_t expected = 0;
        int i_am_leader = atomic_compare_exchange_strong_explicit((_Atomic(uintptr_t) *)
            &current_meta->migration_leader,
            &expected, _Py_ThreadId(), memory_order_acq_rel, memory_order_acquire);
        if (i_am_leader) {
            return AtomicDict_LeaderMigrate(self, current_meta);
        }
    }

    AtomicDict_FollowerMigrate(current_meta);

    return 1;
}

int
AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDictMeta *current_meta /* borrowed */)
{
    int holding_sync_lock = 0;
    AtomicDictMeta *new_meta;
    uint8_t to_log_size = current_meta->log_size + 1;

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

    // pages
    AtomicDict_BeginSynchronousOperation(self);
    holding_sync_lock = 1;
    int ok = AtomicDictMeta_CopyPages(current_meta, new_meta);

    if (ok < 0)
        goto fail;

    for (int64_t page_i = 0; page_i <= new_meta->greatest_allocated_page; ++page_i) {
        Py_INCREF(new_meta->pages[page_i]);
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

#ifdef CEREGGII_DEBUG
    int64_t inserted_before_migration = 0;
    int64_t tombstones_before_migration = 0;
    AtomicDictAccessorStorage *accessor;
    FOR_EACH_ACCESSOR(self, accessor) {
        int64_t local_inserted = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
        int64_t local_tombstones = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, memory_order_acquire);
        inserted_before_migration += local_inserted;
        tombstones_before_migration += local_tombstones;
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_inserted, 0, memory_order_release);
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, 0, memory_order_release);
    }
#endif

    atomic_store_explicit((_Atomic (Py_tss_t *) *) &current_meta->accessor_key, self->accessor_key, memory_order_release);

    for (int64_t page = 0; page <= new_meta->greatest_allocated_page; ++page) {
        new_meta->pages[page]->generation = new_meta->generation;
    }

    // 👀
    Py_INCREF(new_meta);
    atomic_store_explicit((_Atomic (AtomicDictMeta *) *) &current_meta->new_gen_metadata, new_meta, memory_order_release);
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);
    cereggii_unused_in_release_build(set);

#ifdef CEREGGII_DEBUG
    assert(holding_sync_lock);
    int64_t inserted_after_migration = 0;
    FOR_EACH_ACCESSOR(self, accessor) {
        inserted_after_migration += atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
    }
    assert(inserted_after_migration == inserted_before_migration - tombstones_before_migration);
#endif

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
AtomicDict_FollowerMigrate(AtomicDictMeta *current_meta)
{
    AtomicEvent_Wait(current_meta->new_metadata_ready);
    AtomicDictMeta *new_meta = atomic_load_explicit((_Atomic (AtomicDictMeta *) *) &current_meta->new_gen_metadata, memory_order_acquire);

    AtomicDict_CommonMigrate(current_meta, new_meta);

    AtomicEvent_Wait(current_meta->migration_done);
}

void
AtomicDict_CommonMigrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta)
{
    if (AtomicEvent_IsSet(current_meta->node_migration_done))
        return;

    Py_tss_t *ak = atomic_load_explicit((_Atomic(Py_tss_t *) *) &current_meta->accessor_key, memory_order_acquire);
    AtomicDictAccessorStorage *storage = AtomicDict_GetAccessorStorage(ak);
    assert(storage != NULL);

    int64_t *participants = atomic_load_explicit((_Atomic(int64_t *) *) &current_meta->participants, memory_order_acquire);
    int64_t *participant = &participants[storage->accessor_ix];
    assert(participant != NULL);

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

inline void
AtomicDict_MigrateNode(AtomicDictNode *node, AtomicDictMeta *new_meta, const uint64_t trailing_cluster_start, const uint64_t trailing_cluster_size)
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
initialize_in_new_meta(AtomicDictMeta *new_meta, const uint64_t start, const uint64_t end)
{
    // initialize slots in range [start, end)
    for (uint64_t j = 2 * start; j < 2 * (end + 1); ++j) {
        AtomicDict_WriteRawNodeAt(j & (SIZE_OF(new_meta) - 1), 0, new_meta);
    }
}

#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 4096


int64_t
to_migrate(AtomicDictMeta *current_meta, int64_t start_of_block, uint64_t end_of_block)
{
    uint64_t current_size = SIZE_OF(current_meta);
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;
    int64_t to_migrate = 0;
    AtomicDictNode node = {0};

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
AtomicDict_BlockWiseMigrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta, int64_t start_of_block)
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

    AtomicDictNode node = {0};

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

        if (AtomicDict_IsEmpty(&node)) {
            start_of_cluster = i + 1;
            cluster_size = 0;
            continue;
        }
        cluster_size++;
        if (AtomicDict_IsTombstone(&node))
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
        if (AtomicDict_IsEmpty(&node)) {
            break;
        }
        j++;
    }
    if (j > end_of_block) {
        initialize_in_new_meta(new_meta, end_of_block, j - 1);
        while (1) {
            AtomicDict_ReadNodeAt(i & current_size_mask, &node, current_meta);
            if (AtomicDict_IsEmpty(&node)) {
                break;
            }
            cluster_size++;
            if (!AtomicDict_IsTombstone(&node)) {
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
AtomicDict_MigrateNodes(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta)
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
AtomicDict_NodesMigrationDone(AtomicDictMeta *current_meta)
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
