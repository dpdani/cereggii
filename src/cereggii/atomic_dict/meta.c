// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


/**
 * previous_blocks may be NULL.
 */
AtomicDict_Meta *
AtomicDictMeta_New(uint8_t log_size, AtomicDict_Meta *previous_meta)
{
    if (log_size > 25) {
        PyErr_SetString(PyExc_NotImplementedError, "log_size > 25. see https://github.com/dpdani/cereggii/issues/3");
        return NULL;
    }

    AtomicDict_NodeSizeInfo node_sizes = AtomicDict_NodeSizesTable[log_size];

    PyObject *generation = PyObject_CallObject((PyObject *) &PyBaseObject_Type, NULL);
    if (generation == NULL)
        goto fail;

    uint64_t *index = PyMem_RawMalloc(node_sizes.node_size / 8 * (1 << log_size));
    if (index == NULL)
        goto fail;

    AtomicDict_Meta *meta = PyObject_New(AtomicDict_Meta, &AtomicDictMeta_Type);
    if (meta == NULL)
        goto fail;
    PyObject_Init((PyObject *) meta, &AtomicDictMeta_Type);

    meta->log_size = log_size;
    meta->size = 1UL << log_size;
    meta->generation = generation;
    meta->index = index;
    meta->is_compact = 1;
    meta->node_size = node_sizes.node_size;
    meta->distance_size = node_sizes.distance_size;
    meta->max_distance = (1 << meta->distance_size) - 1;
    meta->tag_size = node_sizes.tag_size;
    meta->nodes_in_region = 8 / (meta->node_size / 8);
//#if defined(__aarch64__) && !defined(__APPLE__)
//    meta->nodes_in_zone = 8 / (meta->node_size / 8);
//#else
//    meta->nodes_in_zone = 16 / (meta->node_size / 8);
//#endif
    meta->nodes_in_zone = 16 / (meta->node_size / 8);
    meta->node_mask = (1UL << node_sizes.node_size) - 1;
    meta->index_mask = ((1UL << log_size) - 1) << (node_sizes.node_size - log_size);
    meta->distance_mask = ((1UL << node_sizes.distance_size) - 1) << node_sizes.tag_size;
    meta->tag_mask = (1UL << node_sizes.tag_size) - 1;
    switch (node_sizes.node_size) {
        case 8:
            meta->shift_mask = 8 - 1;
            meta->read_nodes_in_region = AtomicDict_Read8NodesAt;
            meta->read_nodes_in_zone = AtomicDict_Read16NodesAt;
            break;
        case 16:
            meta->shift_mask = 4 - 1;
            meta->read_nodes_in_region = AtomicDict_Read4NodesAt;
            meta->read_nodes_in_zone = AtomicDict_Read8NodesAt;
            break;
        case 32:
            meta->shift_mask = 2 - 1;
            meta->read_nodes_in_region = AtomicDict_Read2NodesAt;
            meta->read_nodes_in_zone = AtomicDict_Read4NodesAt;
            break;
        case 64:
            meta->shift_mask = 1 - 1;
            meta->read_nodes_in_region = AtomicDict_Read1NodeAt;
            meta->read_nodes_in_zone = AtomicDict_Read2NodesAt;
            break;
    }

    meta->tombstone.distance = 0;
    meta->tombstone.index = 0;
    meta->tombstone.tag = 0;
    AtomicDict_ComputeRawNode(&meta->tombstone, meta);

    meta->zero.distance = meta->max_distance;
    meta->zero.index = 0;
    meta->zero.tag = 0;
    AtomicDict_ComputeRawNode(&meta->zero, meta);

    meta->new_gen_metadata = NULL;
    meta->migration_leader = 0;
    meta->copy_nodes_locks = NULL;

    meta->new_metadata_ready = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->new_metadata_ready == NULL)
        goto fail;
    meta->copy_nodes_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->copy_nodes_done == NULL)
        goto fail;
    meta->compaction_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->compaction_done == NULL)
        goto fail;
    meta->node_migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->node_migration_done == NULL)
        goto fail;
    meta->migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->migration_done == NULL)
        goto fail;

    return meta;
    fail:
    Py_XDECREF(generation);
    Py_XDECREF(meta);
    PyMem_RawFree(index);
    return NULL;
}

void
AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta)
{
    memset(meta->index, 0, meta->node_size / 8 * meta->size);
}

void
AtomicDictMeta_CopyIndex(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    memcpy(to_meta->index, from_meta->index, from_meta->node_size / 8 * from_meta->size);
}

int
AtomicDictMeta_InitBlocks(AtomicDict_Meta *meta)
{
    AtomicDict_Block **blocks = NULL;
    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    blocks = PyMem_RawMalloc(sizeof(AtomicDict_Block *) * (meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK));
    if (blocks < 0)
        goto fail;

    blocks[0] = NULL;
    meta->blocks = blocks;
    meta->inserting_block = -1;
    meta->greatest_allocated_block = -1;
    meta->greatest_deleted_block = -1;
    meta->greatest_refilled_block = -1;

    return 0;

    fail:
    return -1;
}

int
AtomicDictMeta_CopyBlocks(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    assert(from_meta != NULL);
    assert(to_meta != NULL);
    assert(from_meta->size <= to_meta->size);

    AtomicDict_Block **previous_blocks = from_meta->blocks;
    int64_t inserting_block = from_meta->inserting_block;
    int64_t greatest_allocated_block = from_meta->greatest_allocated_block;
    int64_t greatest_deleted_block = from_meta->greatest_deleted_block;
    int64_t greatest_refilled_block = from_meta->greatest_refilled_block;


    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    AtomicDict_Block **blocks = PyMem_RawRealloc(previous_blocks,
                                                 sizeof(AtomicDict_Block *) *
                                                 (to_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK));
    if (blocks == NULL)
        goto fail;

    if (previous_blocks == NULL) {
        blocks[0] = NULL;
    } else if (greatest_allocated_block == to_meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
        blocks[greatest_allocated_block + 1] = NULL;
    }

    to_meta->blocks = blocks;

    to_meta->inserting_block = inserting_block;
    to_meta->greatest_allocated_block = greatest_allocated_block;
    to_meta->greatest_deleted_block = greatest_deleted_block;
    to_meta->greatest_refilled_block = greatest_refilled_block;

    return 1;

    fail:
    return -1;
}

void
AtomicDictMeta_ShrinkBlocks(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    memcpy(
        to_meta->blocks,
        from_meta->blocks,
        to_meta->greatest_refilled_block
    );
    memcpy(
        &to_meta->blocks[to_meta->greatest_refilled_block],
        from_meta->blocks[to_meta->greatest_deleted_block + 1],
        to_meta->greatest_allocated_block - to_meta->greatest_deleted_block
    );
}

void
AtomicDictMeta_dealloc(AtomicDict_Meta *self)
{
    // not gc tracked (?)
    uint64_t *index = self->index;
    if (index != NULL) {
        self->index = NULL;
        PyMem_RawFree(index);
    }
    uint8_t *copy_nodes_locks = self->copy_nodes_locks;
    if (copy_nodes_locks != NULL) {
        self->copy_nodes_locks = NULL;
        PyMem_RawFree(copy_nodes_locks);
    }
    Py_CLEAR(self->generation);
    Py_CLEAR(self->new_metadata_ready);
    Py_CLEAR(self->copy_nodes_done);
    Py_CLEAR(self->compaction_done);
    Py_CLEAR(self->node_migration_done);
    Py_CLEAR(self->migration_done);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
