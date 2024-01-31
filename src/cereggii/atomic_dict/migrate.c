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
    AtomicDict_Meta *meta = NULL;
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int migrate = AtomicDict_Migrate(self, meta, meta->log_size, meta->log_size);
    Py_DECREF(meta);
    return migrate;

    fail:
    Py_XDECREF(meta);
    return -1;
}

int
AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta /* borrowed */, uint8_t from_log_size,
                   uint8_t to_log_size)
{
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
    AtomicDict_Meta *new_meta = NULL;
    new_meta = AtomicDictMeta_New(to_log_size, current_meta);
    if (new_meta == NULL)
        goto fail;

    if (from_log_size == to_log_size) {
        AtomicDictMeta_CopyIndex(current_meta, new_meta);
    } else {
        AtomicDictMeta_ClearIndex(new_meta);

        assert(current_meta->copy_nodes_locks == NULL);
        current_meta->copy_nodes_locks = PyMem_RawMalloc(current_meta->size >> 6);  // freed in AtomicDictMeta_dealloc

        if (current_meta->copy_nodes_locks == NULL)
            goto fail;

        memset(current_meta->copy_nodes_locks, 0, current_meta->size >> 6);
    }

    // blocks
    if (from_log_size < to_log_size) {
        AtomicDictMeta_CopyBlocks(current_meta, new_meta);
    } else {
        AtomicDictMeta_InitBlocks(new_meta);
        new_meta->inserting_block = current_meta->inserting_block;
        new_meta->greatest_allocated_block = current_meta->greatest_allocated_block;
        new_meta->greatest_deleted_block = current_meta->greatest_deleted_block;
        new_meta->greatest_refilled_block = current_meta->greatest_refilled_block;
        AtomicDictMeta_ShrinkBlocks(current_meta, new_meta);
    }

    // 👀
    current_meta->new_gen_metadata = new_meta;
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    AtomicDict_CommonMigrate(current_meta, new_meta);

    // 🎉
    assert(
        AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta)
    );
    AtomicEvent_Set(current_meta->migration_done);

    return 1;

    fail:
    return -1;
}

void
AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    if (current_meta->size < new_meta->size) {
        if (!AtomicEvent_IsSet(current_meta->copy_nodes_done)) {
            int copy_nodes_done = AtomicDict_MigrateCopyNodes(current_meta, new_meta);

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

    if (current_meta->log_size == new_meta->log_size) {
        return;
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

    for (copy_lock = 0; copy_lock < current_meta->size >> 6; ++copy_lock) {
        uint64_t lock = (copy_lock + tid) % (current_meta->size >> 6);

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

    for (copy_lock = 0; copy_lock < current_meta->size >> 6; ++copy_lock) {
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

    for (uint64_t i = 0; i < current_meta->size; ++i) {
        uint64_t head = (i + tid) % current_meta->size;
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
}

int
AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t tid = _Py_ThreadId();

    uint64_t block_i;
    AtomicDict_Block *block;

    for (uint64_t i = 0; i <= new_meta->greatest_allocated_block; ++i) {
        block_i = (i + tid) % (new_meta->greatest_allocated_block + 1);

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

        AtomicDict_Node nodes[ATOMIC_DICT_ENTRIES_IN_BLOCK];
        uint64_t positions[ATOMIC_DICT_ENTRIES_IN_BLOCK];
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
                    nodes[entry_i].distance = sr.node.distance;
                    nodes[entry_i].index = sr.node.index;
                    nodes[entry_i].tag = entry.hash;
                    positions[entry_i] = sr.position;
                    if (entry.flags & ENTRY_FLAGS_COMPACTED) {
                        block->entries[entry_i].flags &= ~ENTRY_FLAGS_COMPACTED;
                        block->entries[entry_i].flags &= ~ENTRY_FLAGS_LOCKED;
                    }
                } else {
                    nodes[entry_i].index = 0;
                }
            } else {
                nodes[entry_i].index = 0;
            }
        }

        for (entry_i = 0; entry_i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++entry_i) {
            if (nodes[entry_i].index != 0) {
                uint64_t ix = AtomicDict_Distance0Of((Py_hash_t) nodes[entry_i].tag, new_meta)
                              + nodes[entry_i].distance;

                AtomicDict_WriteNodeAt(ix, &nodes[entry_i], new_meta);
            }
        }

        block->generation = new_meta->generation;
    }

    int done = 1;
    for (uint64_t i = 0; i < new_meta->greatest_allocated_block; ++i) {
        if (new_meta->greatest_refilled_block < block_i && block_i <= new_meta->greatest_deleted_block)
            continue;

        if (new_meta->blocks[i]->generation != new_meta->generation) {
            done = 0;
            break;
        }
    }

    return done;
}
