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
AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *current_meta)
{
    if (current_meta->migration_leader == 0) {
        return 0;
    }

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
    if (from_log_size < to_log_size) {
        AtomicDictMeta_CopyBlocks(current_meta, new_meta);
    } else {
        AtomicDictMeta_InitBlocks(new_meta);
        AtomicDictMeta_ShrinkBlocks(self, current_meta, new_meta);
    }

    for (uint64_t block_i = 0; block_i <= new_meta->greatest_allocated_block; ++block_i) {
        Py_INCREF(new_meta->blocks[block_i]);
    }

    AtomicDictMeta_ClearIndex(new_meta);

    // 👀
    Py_INCREF(new_meta);
    current_meta->new_gen_metadata = new_meta;
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);
    AtomicEvent_Set(current_meta->migration_done);
    Py_DECREF(new_meta);  // this may seem strange: why decref'ing the new meta?
    // the reason is that AtomicRef_CompareAndSet also increases new_meta's refcount,
    // which is exactly what we want. but the reference count was already 1, as it
    // was set during the initialization of new_meta. that's what we're decref'ing
    // for in here.

    return 1;

    fail:
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
    if (!AtomicEvent_IsSet(current_meta->node_migration_done)) {
        int node_migration_done = AtomicDict_MigrateReInsertAll(current_meta, new_meta);

        if (node_migration_done) {
            AtomicEvent_Set(current_meta->node_migration_done);
        }
        AtomicEvent_Wait(current_meta->node_migration_done);
    }
}

int
AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t tid = _Py_ThreadId();

    int64_t copy_lock;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        uint64_t lock = (copy_lock + tid) % (new_meta->greatest_allocated_block + 1);

        int locked = CereggiiAtomic_CompareExchangePtr((void **) &new_meta->blocks[lock]->generation, current_meta->generation, NULL);
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
                                                                     &entry_loc, &must_grow);
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
