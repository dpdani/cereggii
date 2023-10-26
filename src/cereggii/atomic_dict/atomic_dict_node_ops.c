// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN


// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread local copy) and
// then pass a reference to the copy to these functions instead.


void
atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta)
{
    node->node =
        (node->index << (meta->node_size - meta->log_size))
        | (meta->distance_mask - (node->distance << meta->tag_size))
        | (node->tag & meta->tag_mask);
}

void
atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta)
{

    unsigned long shift = ix & meta->shift_mask;
    unsigned long node_region = meta->index[ix & ~meta->shift_mask];
    unsigned long node_raw = (node_region & (meta->node_mask << (shift * meta->node_size))) >> (shift * meta->node_size);
    node->node = node_raw;
    node->index = (node_raw & meta->index_mask) >> (meta->node_size - meta->log_size);
    node->distance = (~(node_raw & meta->distance_mask) & meta->distance_mask) >> meta->tag_size;
    node->tag = node_raw & meta->tag_mask;
}

int
atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta)
{
    atomic_dict_compute_raw_node(node, meta);
    unsigned long shift = ix & meta->shift_mask;
    unsigned long node_raw = meta->index[ix & ~meta->shift_mask];
    node_raw &= ~(meta->node_mask << (shift * meta->node_size));
    node_raw |= node->node << (shift * meta->node_size);
    meta->index[ix & ~meta->shift_mask] = node_raw;
}
