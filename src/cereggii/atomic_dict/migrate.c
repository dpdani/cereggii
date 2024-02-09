// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"


int
AtomicDict_Grow(AtomicDict *self)
{
    AtomicDict_Meta *meta = NULL;
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size + 1);
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

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size - 1);
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
         * always do one migration with equal log_sizes first to reduce the blocks.
         * then, if the resulting meta is not compact, increase the log_size
         * and migrate, until meta is compact.
         */

        meta = NULL;
        meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
        if (meta == NULL)
            goto fail;

        migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size + !is_compact);
        Py_DECREF(meta);
        if (migrate < 0)
            goto fail;

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

    Py_INCREF(Py_None);
    Py_RETURN_NONE;
}


int
AtomicDict_MaybeHelpMigrate(AtomicDict *self, AtomicDict_Meta *current_meta)
{
    if (current_meta->migration_leader == 0) {
        return 0;
    }

    AtomicDict_Meta *new_meta = NULL;

    AtomicEvent_Wait(current_meta->new_metadata_ready);
    new_meta = current_meta->new_gen_metadata;

    return AtomicDict_Migrate(self, current_meta, current_meta->log_size, new_meta->log_size);
}


int
AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta /* borrowed */, uint8_t from_log_size,
                   uint8_t to_log_size)
{
    assert(to_log_size <= from_log_size + 1);
    if (current_meta->migration_leader == 0) {
        int i_am_leader = CereggiiAtomic_CompareExchangeUIntPtr(
            &current_meta->migration_leader,
            0, _Py_ThreadId());
        if (i_am_leader) {
            return AtomicDict_LeaderMigrate(self, current_meta, from_log_size, to_log_size);
        }
    }

    AtomicEvent_Wait(current_meta->new_metadata_ready);
    AtomicDict_Meta *new_meta = current_meta->new_gen_metadata;
    AtomicDict_CommonMigrate(current_meta, new_meta);
    AtomicEvent_Wait(current_meta->migration_done);

    return 1;
}

int
AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDict_Meta *current_meta /* borrowed */, uint8_t from_log_size,
                         uint8_t to_log_size)
{
    AtomicDict_Meta *new_meta;
    beginning:
    new_meta = NULL;
    new_meta = AtomicDictMeta_New(to_log_size, current_meta);
    if (new_meta == NULL)
        goto fail;

    if (to_log_size > ATOMIC_DICT_MAX_LOG_SIZE) {
        PyErr_SetString(PyExc_ValueError, "can hold at most 2^56 items.");
        goto fail;
    }
    if (to_log_size < self->min_log_size) {
        to_log_size = self->min_log_size;
    }

    // blocks
    if (from_log_size < to_log_size) {
        AtomicDictMeta_CopyBlocks(current_meta, new_meta);
    } else {
        AtomicDictMeta_InitBlocks(new_meta);
        AtomicDictMeta_ShrinkBlocks(self, current_meta, new_meta);
    }

    if (from_log_size == to_log_size) {
        AtomicDictMeta_CopyIndex(current_meta, new_meta);
    } else {
        AtomicDictMeta_ClearIndex(new_meta);

        assert(current_meta->copy_nodes_locks == NULL);
        current_meta->copy_nodes_locks = PyMem_RawMalloc(  // freed in AtomicDictMeta_dealloc
            /**
             * this is a comfy size; it is both:
             *   - fine-grained locking system over the index, dividing it into
             *     chunks of arbitrary size (ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK); and
             *   - a throwaway (i.e. one-use-only) locking system over the blocks,
             *     because there's exactly one lock per block.
             *
             * thus, we can use it both for AtomicDict_MigrateCopyNodes, and for
             * AtomicDict_MigrateReInsertAll.
             */
            current_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);

        if (current_meta->copy_nodes_locks == NULL)
            goto fail;

        memset(current_meta->copy_nodes_locks, 0, current_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);
    }

    // 👀
    current_meta->new_gen_metadata = new_meta;
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);
    AtomicEvent_Set(current_meta->migration_done);

    return 1;

    fail:
    // don't block other threads indefinitely
    AtomicEvent_Set(current_meta->migration_done);
    AtomicEvent_Set(current_meta->node_migration_done);
    AtomicEvent_Set(current_meta->compaction_done);
    AtomicEvent_Set(current_meta->copy_nodes_done);
    AtomicEvent_Set(current_meta->new_metadata_ready);
    return -1;
}

void
AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    if (current_meta->size != new_meta->size) {
        if (!AtomicEvent_IsSet(current_meta->copy_nodes_done)) {
            int copy_nodes_done;

            if (current_meta->size < new_meta->size) {
                copy_nodes_done = AtomicDict_MigrateCopyNodes(current_meta, new_meta);
            } else { // => current_meta->size > new_meta->size
                /**
                 * we don't know whether there will be space to copy all nodes
                 * (including reservations and tombstones), thus we need to
                 * actually re-insert the nodes using the usual routines.
                 * furthermore, nodes that weren't colliding before may collide
                 * in the smaller table.
                 */
                copy_nodes_done = AtomicDict_MigrateReInsertAll(current_meta, new_meta);
            }

            if (copy_nodes_done) {
                AtomicEvent_Set(current_meta->copy_nodes_done);
            }
            AtomicEvent_Wait(current_meta->copy_nodes_done);
        }
    }

    if (!AtomicEvent_IsSet(current_meta->compaction_done)) {
        int compaction_done = AtomicDict_MigrateCompact(current_meta, new_meta);
        if (compaction_done) {
            AtomicEvent_Set(current_meta->compaction_done);
        }
        AtomicEvent_Wait(current_meta->compaction_done);
    }

    if (!AtomicEvent_IsSet(current_meta->node_migration_done)) {
        int node_migration_done = AtomicDict_MigrateNodes(current_meta, new_meta);

        if (node_migration_done) {
            AtomicEvent_Set(current_meta->node_migration_done);
        }
        AtomicEvent_Wait(current_meta->node_migration_done);
    }
}


/// sub-routines

int
AtomicDict_MigrateCopyNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    assert(current_meta->size < new_meta->size);
    assert(current_meta->copy_nodes_locks != NULL);
    uint64_t tid = _Py_ThreadId();

    AtomicDict_BufferedNodeReader current;
    current.zone = -1;
    uint64_t copy_lock;

    for (copy_lock = 0; copy_lock < current_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK; ++copy_lock) {
        uint64_t lock = (copy_lock + tid) % (current_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);

        int locked = CereggiiAtomic_CompareExchangeUInt8(&current_meta->copy_nodes_locks[lock], 0, 1);
        if (!locked)
            continue;

        for (uint64_t i = 0; i < current_meta->size; ++i) {
            AtomicDict_ReadNodesFromZoneIntoBuffer(i, &current, current_meta);

            if (current.node.node != 0) {
                AtomicDict_WriteNodeAt(i, &current.node, new_meta);
            }
        }

        current_meta->copy_nodes_locks[lock] = 2; // mark as done
    }

    int done = 1;

    for (copy_lock = 0; copy_lock < current_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK; ++copy_lock) {
        if (current_meta->copy_nodes_locks[copy_lock] != 2) {
            done = 0;
            break;
        }
    }

    return done;
}

int
AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    assert(current_meta->copy_nodes_locks != NULL);
    uint64_t tid = _Py_ThreadId();

    int64_t copy_lock;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        uint64_t lock = (copy_lock + tid) % (new_meta->greatest_allocated_block + 1);

        int locked = CereggiiAtomic_CompareExchangeUInt8(&current_meta->copy_nodes_locks[lock], 0, 1);
        if (!locked)
            continue;

        if (new_meta->greatest_refilled_block < copy_lock && copy_lock <= new_meta->greatest_deleted_block)
            goto mark_as_done;

        AtomicDict_EntryLoc entry_loc;

        for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
            entry_loc.location = (copy_lock << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) + i;
            entry_loc.entry = AtomicDict_GetEntryAt(entry_loc.location, new_meta);

            if (entry_loc.entry->key == NULL || entry_loc.entry->value == NULL ||
                entry_loc.entry->flags & ENTRY_FLAGS_TOMBSTONE || entry_loc.entry->flags & ENTRY_FLAGS_SWAPPED)
                continue;

            AtomicDict_InsertedOrUpdated result = AtomicDict_InsertOrUpdate(new_meta, &entry_loc);
            assert(result == inserted);
        }

        mark_as_done:
        current_meta->copy_nodes_locks[lock] = 2; // mark as done
    }

    int done = 1;

    for (copy_lock = 0; copy_lock <= new_meta->greatest_allocated_block; ++copy_lock) {
        if (current_meta->copy_nodes_locks[copy_lock] != 2) {
            done = 0;
            break;
        }
    }

    return done;
}

int
AtomicDict_MigrateCompact(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t tid = _Py_ThreadId();

    AtomicDict_BufferedNodeReader reader;
    reader.zone = -1;

    uint64_t start = tid % new_meta->size;

    for (; start < new_meta->size; ++start) {
        AtomicDict_ReadNodesFromZoneIntoBuffer(start, &reader, new_meta);

        if (reader.node.node == 0)
            break;
    }

    uint64_t i = 0;
    for (; i < new_meta->size; ++i) {
        uint64_t head = (i + start) % new_meta->size;
        AtomicDict_ReadNodesFromZoneIntoBuffer(head, &reader, new_meta);
        AtomicDict_Node head_node = reader.node;

        if (head_node.node == 0 || head_node.distance > 0)
            continue;

        uint64_t length = 0;
        while (reader.node.node != 0) {
            length++;

            AtomicDict_ReadNodesFromZoneIntoBuffer(head + length, &reader, new_meta);
        }

        i += length;

        if (length == 1)
            continue;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(head_node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.flags & ENTRY_FLAGS_LOCKED || entry.flags & ENTRY_FLAGS_COMPACTED)
            continue;

        int locked = CereggiiAtomic_CompareExchangeUInt8(&entry_p->flags, entry.flags,
                                                         entry.flags | ENTRY_FLAGS_LOCKED);

        if (!locked)
            continue;

        AtomicDict_RobinHoodCompact(current_meta, new_meta, head, length);

        while (!CereggiiAtomic_CompareExchangeUInt8(&entry_p->flags,
                                                    entry.flags | ENTRY_FLAGS_LOCKED,
                                                    entry.flags | ENTRY_FLAGS_LOCKED | ENTRY_FLAGS_COMPACTED)) {
            AtomicDict_ReadEntry(entry_p, &entry);
        }
    }

    for (i = 0; i < new_meta->size; ++i) {
        uint64_t head = i;
        AtomicDict_ReadNodesFromZoneIntoBuffer(head, &reader, new_meta);
        AtomicDict_Node head_node = reader.node;

        if (head_node.node == 0 || head_node.distance > 0)
            continue;

        uint64_t length = 0;
        while (reader.node.node != 0) {
            length++;

            AtomicDict_ReadNodesFromZoneIntoBuffer(head + length, &reader, new_meta);
        }

        i += length;

        if (length == 1)
            continue;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(head_node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.flags & ENTRY_FLAGS_LOCKED && !(entry.flags & ENTRY_FLAGS_COMPACTED))
            return 0;
    }

    return 1;
}

int
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t tid = _Py_ThreadId();

    int64_t block_i;
    AtomicDict_Block *block;

    for (uint64_t i = 0; i <= new_meta->greatest_allocated_block; ++i) {
        block_i = (int64_t) (i + tid) % (new_meta->greatest_allocated_block + 1);

        if (new_meta->greatest_refilled_block < block_i && block_i <= new_meta->greatest_deleted_block)
            continue;

        block = new_meta->blocks[block_i];
        PyObject *block_gen = block->generation;

        if (block_gen == new_meta->generation || block_gen == NULL)
            continue;

        assert(block_gen == current_meta->generation);

        int block_locked = CereggiiAtomic_CompareExchangePtr((void **) &block->generation, current_meta->generation,
                                                             NULL);
        if (!block_locked)
            continue;

        AtomicDict_Node node;
        AtomicDict_Entry entry;
        AtomicDict_SearchResult sr;
        int entry_i;

        for (entry_i = 0; entry_i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++entry_i) {
            AtomicDict_ReadEntry(&block->entries[entry_i], &entry);

            if (entry.value != NULL
                && !(entry.flags & ENTRY_FLAGS_TOMBSTONE)
                && !(entry.flags & ENTRY_FLAGS_SWAPPED)
                ) {
                AtomicDict_LookupEntry(new_meta, (block_i << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) + entry_i,
                                       entry.hash, &sr);
                if (sr.found) {
                    node.distance = sr.node.distance;
                    node.index = sr.node.index;
                    node.tag = entry.hash;
                    if (new_meta->is_compact && AtomicDict_NodeIsReservation(&node, new_meta)) {
                        CereggiiAtomic_StoreUInt8(&new_meta->is_compact, 0);
                    }
                    AtomicDict_WriteNodeAt(sr.position, &node, new_meta);
                    if (entry.flags & ENTRY_FLAGS_COMPACTED) {
                        block->entries[entry_i].flags &= ~ENTRY_FLAGS_COMPACTED;
                        block->entries[entry_i].flags &= ~ENTRY_FLAGS_LOCKED;
                    }
                }
            }
        }

        block->generation = new_meta->generation;
    }

    int done = 1;
    for (int64_t i = 0; i < new_meta->greatest_allocated_block; ++i) {
        if (new_meta->greatest_refilled_block < i && i <= new_meta->greatest_deleted_block)
            continue;

        if (new_meta->blocks[i]->generation != new_meta->generation) {
            done = 0;
            break;
        }
    }

    return done;
}
