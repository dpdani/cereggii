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
atomic_dict_meta *
atomic_dict_new_meta(unsigned char log_size, atomic_dict_meta *previous_meta)
{
    node_size_info node_sizes = node_sizes_table[log_size];

    PyObject *generation = PyObject_CallObject((PyObject *) &PyBaseObject_Type, NULL);
    if (generation == NULL)
        goto fail;

    unsigned long *index = PyMem_RawMalloc(node_sizes.node_size / 8 * (1 << log_size));
    if (index == NULL)
        goto fail;
    memset(index, 0, node_sizes.node_size / 8 * (1 << log_size));

    atomic_dict_block **previous_blocks = NULL;
    long inserting_block = -1;
    long greatest_allocated_block = -1;
    long greatest_deleted_block = -1;
    long greatest_refilled_block = -1;

    if (previous_meta != NULL) {
        previous_blocks = previous_meta->blocks;
        inserting_block = previous_meta->inserting_block;
        greatest_allocated_block = previous_meta->greatest_allocated_block;
        greatest_deleted_block = previous_meta->greatest_deleted_block;
        greatest_refilled_block = previous_meta->greatest_refilled_block;
    }

    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    atomic_dict_block **blocks = PyMem_RawRealloc(previous_blocks,
                                                  sizeof(atomic_dict_block *) * ((1 << log_size) >> 6));
    if (blocks == NULL)
        goto fail;

    if (previous_blocks == NULL) {
        blocks[0] = NULL;
    } else if (greatest_allocated_block == (1 << log_size) >> 6) {
        blocks[greatest_allocated_block + 1] = NULL;
    }

    atomic_dict_meta *meta = PyObject_New(atomic_dict_meta, &AtomicDictMeta);
    if (meta == NULL)
        goto fail;
    PyObject_Init((PyObject *) meta, &AtomicDictMeta);

    meta->log_size = log_size;
    meta->generation = generation;
    meta->index = index;
    meta->blocks = blocks;
    meta->node_size = node_sizes.node_size;
    meta->distance_size = node_sizes.distance_size;
    meta->tag_size = node_sizes.tag_size;
    meta->nodes_in_region = 8 / (meta->node_size / 8);
    meta->nodes_in_two_regions = 16 / (meta->node_size / 8);
    meta->node_mask = (1UL << node_sizes.node_size) - 1;
    meta->index_mask = ((1UL << log_size) - 1) << (node_sizes.node_size - log_size);
    meta->distance_mask = ((1UL << node_sizes.distance_size) - 1) << node_sizes.tag_size;
    meta->tag_mask = (1UL << node_sizes.tag_size) - 1;
    switch (node_sizes.node_size) {
        case 8:
            meta->shift_mask = 8 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_8_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_16_nodes_at;
            break;
        case 16:
            meta->shift_mask = 4 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_4_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_8_nodes_at;
            break;
        case 32:
            meta->shift_mask = 2 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_2_nodes_at;
            meta->read_double_word_nodes_at = atomic_dict_read_4_nodes_at;
            break;
        case 64:
            meta->shift_mask = 1 - 1;
            meta->read_single_word_nodes_at = atomic_dict_read_1_node_at;
            meta->read_double_word_nodes_at = atomic_dict_read_2_nodes_at;
            break;
    }

    meta->inserting_block = inserting_block;
    meta->greatest_allocated_block = greatest_allocated_block;
    meta->greatest_deleted_block = greatest_deleted_block;
    meta->greatest_refilled_block = greatest_refilled_block;

    return meta;
    fail:
    Py_XDECREF(generation);
    Py_XDECREF(meta);
    PyMem_RawFree(index);
    PyMem_RawFree(blocks);
    return NULL;
}

void
atomic_dict_meta_dealloc(atomic_dict_meta *self)
{
    // not gc tracked (?)
    PyMem_RawFree(self->index);
    Py_CLEAR(self->generation);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
