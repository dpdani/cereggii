// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


AtomicDict_Meta *
AtomicDictMeta_New(uint8_t log_size)
{
    void *generation = NULL;
    uint64_t *index = NULL;
    AtomicDict_Meta *meta = NULL;

    generation = PyMem_RawMalloc(1);
    if (generation == NULL)
        goto fail;

    index = PyMem_RawMalloc(sizeof(uint64_t) * (1 << log_size));
    if (index == NULL)
        goto fail;

    meta = PyObject_GC_New(AtomicDict_Meta, &AtomicDictMeta_Type);
    if (meta == NULL)
        goto fail;

    meta->blocks = NULL;
    meta->greatest_allocated_block = -1;
    meta->greatest_deleted_block = -1;
    meta->greatest_refilled_block = -1;
    meta->inserting_block = -1;

    meta->log_size = log_size;
    meta->generation = generation;
    meta->index = index;

    meta->new_gen_metadata = NULL;
    meta->migration_leader = 0;
    meta->node_to_migrate = 0;
    meta->accessor_key = NULL;

    meta->new_metadata_ready = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->new_metadata_ready == NULL)
        goto fail;
    meta->node_migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->node_migration_done == NULL)
        goto fail;
    meta->migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->migration_done == NULL)
        goto fail;

    PyObject_GC_Track(meta);
    return meta;
    fail:
    PyMem_RawFree(generation);
    Py_XDECREF(meta);
    if (index != NULL) {
        PyMem_RawFree(index);
    }
    return NULL;
}

void
AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta)
{
    memset(meta->index, 0, sizeof(uint64_t) * SIZE_OF(meta));
}

int
AtomicDictMeta_InitBlocks(AtomicDict_Meta *meta)
{
    AtomicDict_Block **blocks = NULL;
    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    blocks = PyMem_RawMalloc(sizeof(AtomicDict_Block *) * (SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK));
    if (blocks == NULL)
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
    assert(from_meta->log_size <= to_meta->log_size);

    AtomicDict_Block **previous_blocks = from_meta->blocks;
    int64_t inserting_block = from_meta->inserting_block;
    int64_t greatest_allocated_block = from_meta->greatest_allocated_block;
    int64_t greatest_deleted_block = from_meta->greatest_deleted_block;
    int64_t greatest_refilled_block = from_meta->greatest_refilled_block;


    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    AtomicDict_Block **blocks = PyMem_RawMalloc(sizeof(AtomicDict_Block *) *
                                                (SIZE_OF(to_meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK));
    if (blocks == NULL)
        goto fail;

    if (previous_blocks != NULL) {
        for (int64_t block_i = 0; block_i <= greatest_allocated_block; ++block_i) {
            blocks[block_i] = previous_blocks[block_i];
        }
    }

    if (previous_blocks == NULL) {
        blocks[0] = NULL;
    } else {
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
AtomicDictMeta_ShrinkBlocks(AtomicDict *self, AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    to_meta->blocks[0] = from_meta->blocks[0];  // entry 0 must be kept

    int64_t block_j = 1;
    for (int64_t block_i = 1; block_i <= from_meta->greatest_allocated_block; ++block_i) {
        if (from_meta->greatest_refilled_block < block_i && block_i <= from_meta->greatest_deleted_block)
            continue;

        to_meta->blocks[block_j] = from_meta->blocks[block_i];

        for (AtomicDict_AccessorStorage *storage = self->accessors; storage != NULL; storage = storage->next_accessor) {
            AtomicDict_UpdateBlocksInReservationBuffer(&storage->reservation_buffer, block_i, block_j);
        }

        block_j++;
    }
    block_j--;

    to_meta->inserting_block = block_j;
    to_meta->greatest_allocated_block = block_j;

    if (from_meta->greatest_refilled_block > 0) {
        to_meta->greatest_refilled_block = 0;
    } else {
        to_meta->greatest_refilled_block = -1;
    }

    if (from_meta->greatest_deleted_block > 0) {
        to_meta->greatest_deleted_block = 0;
    } else {
        to_meta->greatest_deleted_block = -1;
    }
}

int
AtomicDictMeta_traverse(AtomicDict_Meta *self, visitproc visit, void *arg)
{
    if (self->blocks == NULL)
        return 0;

    for (uint64_t block_i = 0; block_i <= self->greatest_allocated_block; ++block_i) {
        Py_VISIT(self->blocks[block_i]);
    }
    return 0;
}

int
AtomicDictMeta_clear(AtomicDict_Meta *self)
{
    for (uint64_t block_i = 0; block_i <= self->greatest_allocated_block; ++block_i) {
        Py_CLEAR(self->blocks[block_i]);
    }

    Py_CLEAR(self->new_gen_metadata);
    Py_CLEAR(self->new_metadata_ready);
    Py_CLEAR(self->node_migration_done);
    Py_CLEAR(self->migration_done);

    return 0;
}

void
AtomicDictMeta_dealloc(AtomicDict_Meta *self)
{
    PyObject_GC_UnTrack(self);

    AtomicDictMeta_clear(self);

    uint64_t *index = self->index;
    if (index != NULL) {
        self->index = NULL;
        PyMem_RawFree(index);
    }

    if (self->blocks != NULL) {
        PyMem_RawFree(self->blocks);
    }

    PyMem_RawFree(self->generation);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
