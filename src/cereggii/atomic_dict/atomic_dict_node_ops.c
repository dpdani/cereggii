// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdatomic.h>
#include "atomic_dict.h"

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.


inline void
atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta)
{
    node->node =
        (node->index << (meta->node_size - meta->log_size))
        | (meta->distance_mask - (node->distance << meta->tag_size))
        | (node->tag & meta->tag_mask);
}

inline int
atomic_dict_node_is_reservation(atomic_dict_node *node, atomic_dict_meta *meta)
{
    return node->node != 0 && node->distance == (1 << meta->distance_size) - 1;
}

inline unsigned long
region_of(unsigned long ix, atomic_dict_meta *meta)
{
    ix = ix % (1 << meta->log_size);
    return (ix & ~meta->shift_mask) / meta->nodes_in_region;
}

inline unsigned long
shift_in_region_of(unsigned long ix, atomic_dict_meta *meta)
{
    return ix & meta->shift_mask;
}

inline void
atomic_dict_parse_node_from_region(unsigned long ix, unsigned long region, atomic_dict_node *node,
                                   atomic_dict_meta *meta)
{
    unsigned long shift = shift_in_region_of(ix, meta);
    unsigned long node_raw =
        (region & (meta->node_mask << (shift * meta->node_size))) >> (shift * meta->node_size);
    node->node = node_raw;
    node->index = (node_raw & meta->index_mask) >> (meta->node_size - meta->log_size);
    node->distance = (~(node_raw & meta->distance_mask) & meta->distance_mask) >> meta->tag_size;
    node->tag = node_raw & meta->tag_mask;
}

void
atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta)
{
    unsigned long node_region = meta->index[region_of(ix, meta)];
    atomic_dict_parse_node_from_region(ix, node_region, node, meta);
}

/**
 * the following functions read more than one node at a time.
 * NB: they expect all the nodes to be in two successive words of memory!
 * this means that in order to call e.g. atomic_dict_read_8_nodes_at,
 * meta->node_size must be at most 16.
 **/

void
atomic_dict_read_1_node_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    unsigned long node_region = meta->index[region_of(ix, meta)];
    atomic_dict_parse_node_from_region(ix, node_region, &nodes[0], meta);
}

void
atomic_dict_read_2_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    unsigned long little = region_of(ix, meta);
    unsigned long big = region_of(ix + 1, meta);
    unsigned long node_region_little = meta->index[little];
    unsigned long node_region_big = node_region_little;
    if (big != little) {
        node_region_big = meta->index[big];
    }
    atomic_dict_parse_node_from_region(ix, node_region_little, &nodes[0], meta);
    atomic_dict_parse_node_from_region(ix + 1, node_region_big, &nodes[1], meta);
}

void
atomic_dict_read_4_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size <= 32);
    unsigned long little = region_of(ix, meta);
    unsigned long middle = little + 1;
    unsigned long big = region_of(ix + 3, meta);
    unsigned long node_region_little = meta->index[little];
    unsigned long node_region_middle = node_region_little;
    unsigned long node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little) {
        node_region_big = meta->index[big];
    }

    unsigned long region;
    for (int i = 0; i < 4; ++i) {
        unsigned long r = region_of(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        atomic_dict_parse_node_from_region(ix + i, region, &nodes[i], meta);
    }
}

void
atomic_dict_read_8_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size <= 16);
    unsigned long little = region_of(ix, meta);
    unsigned long middle = little + 1;
    unsigned long big = region_of(ix + 7, meta);
    unsigned long node_region_little = meta->index[little];
    unsigned long node_region_middle = node_region_little;
    unsigned long node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little) {
        node_region_big = meta->index[big];
    }

    unsigned long region;
    for (int i = 0; i < 8; ++i) {
        unsigned long r = region_of(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        atomic_dict_parse_node_from_region(ix + i, region, &nodes[i], meta);
    }
}

void
atomic_dict_read_16_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size <= 8);
    unsigned long little = region_of(ix, meta);
    unsigned long middle = little + 1;
    unsigned long big = region_of(ix + 15, meta);
    unsigned long node_region_little = meta->index[little];
    unsigned long node_region_middle = node_region_little;
    unsigned long node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little && big != middle) {
        node_region_big = meta->index[big];
    }

    unsigned long region;
    for (int i = 0; i < 16; ++i) {
        unsigned long r = region_of(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        atomic_dict_parse_node_from_region(ix + i, region, &nodes[i], meta);
    }
}


int
atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta)
{
    atomic_dict_compute_raw_node(node, meta);
    unsigned long shift = ix & meta->shift_mask;
    unsigned long region = region_of(ix, meta);
    unsigned long node_raw = meta->index[region];
    node_raw &= ~(meta->node_mask << (shift * meta->node_size));
    node_raw |= node->node << (shift * meta->node_size);
    meta->index[region] = node_raw;
}

int
atomic_dict_atomic_write_node_at(unsigned long ix, atomic_dict_node *expected, atomic_dict_node *new,
                                 atomic_dict_meta *meta)
{
    unsigned long shift = ix & meta->shift_mask;
    atomic_dict_compute_raw_node(expected, meta);
    atomic_dict_compute_raw_node(new, meta);
    switch (meta->node_size) {
        case 8:
            return __sync_bool_compare_and_swap_1(
                &meta->index[region_of(ix, meta)] + shift,
                expected->node, new->node);
        case 16:
            return __sync_bool_compare_and_swap_2(
                &meta->index[region_of(ix, meta)] + shift,
                expected->node, new->node);
        case 32:
            return __sync_bool_compare_and_swap_4(
                &meta->index[region_of(ix, meta)] + shift,
                expected->node, new->node);
        case 64:
            return __sync_bool_compare_and_swap_8(
                &meta->index[region_of(ix, meta)] + shift,
                expected->node, new->node);
        default:
            assert(0);
    }
}

int
atomic_dict_atomic_write_nodes_at(unsigned long ix, int n, atomic_dict_node *expected, atomic_dict_node *new,
                                  atomic_dict_meta *meta)
{
    assert(n > 0);
    assert(n <= 16);

    unsigned long shift = shift_in_region_of(ix, meta);
    unsigned long little = region_of(ix, meta);
    unsigned long big = region_of(ix + n - 1, meta);
    assert(little <= big <= little + 1); // XXX implement index circular behavior
    unsigned long long expected_raw = 0, new_raw = 0;
    int i;
    for (i = 0; i < n; ++i) {
        atomic_dict_compute_raw_node(&expected[i], meta);
        atomic_dict_compute_raw_node(&new[i], meta);
    }
    for (i = 0; i < n; ++i) {
        expected_raw |= expected[i].node << (meta->node_size * i);
        new_raw |= new[i].node << (meta->node_size * i);
    }

    int n_bytes = (meta->node_size >> 3) * n;
    assert(n_bytes <= 16);
    int must_write;
    if (n_bytes == 1) {
        must_write = 1;
    } else if (n_bytes <= 2) {
        must_write = 2;
    } else if (n_bytes <= 4) {
        must_write = 4;
    } else if (n_bytes <= 8) {
        must_write = 8;
    } else if (n_bytes <= 16) {
        must_write = 16;
    } else {
        assert(0);
    }
    int must_write_nodes = must_write / (meta->node_size / 8);
    for (; i < must_write_nodes; ++i) {
        new_raw |= expected[i].node << (meta->node_size * (meta->node_size - i - 1));
    }

    unsigned char *index_address = (unsigned char *) &meta->index[little] + shift * (meta->node_size / 8);
    switch (must_write) {
        case 1:
            return __sync_bool_compare_and_swap_1(index_address, expected_raw, new_raw);
        case 2:
            return __sync_bool_compare_and_swap_2(index_address, expected_raw, new_raw);
        case 4:
            return __sync_bool_compare_and_swap_4(index_address, expected_raw, new_raw);
        case 8:
            return __sync_bool_compare_and_swap_8(index_address, expected_raw, new_raw);
        case 16:
            return __sync_bool_compare_and_swap_16(index_address, expected_raw, new_raw);
        default:
            assert(0);
    }
}
