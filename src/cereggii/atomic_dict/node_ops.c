// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdatomic.h>
#include "atomic_dict.h"
#include "atomic_dict_internal.h"

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.


inline void
AtomicDict_ComputeRawNode(atomic_dict_node *node, atomic_dict_meta *meta)
{
    assert(node->index < (1 << meta->log_size));
    assert(node->distance <= meta->max_distance);
    node->node =
        (node->index << (meta->node_size - meta->log_size))
        | (meta->distance_mask - ((uint64_t) node->distance << (uint64_t) meta->tag_size))
        | (node->tag & meta->tag_mask);
}

inline int
AtomicDict_NodeIsReservation(atomic_dict_node *node, atomic_dict_meta *meta)
{
    return node->node != 0 && node->distance == (1 << meta->distance_size) - 1;
}

inline uint64_t
AtomicDict_RegionOf(uint64_t ix, atomic_dict_meta *meta)
{
    ix = ix % (1 << meta->log_size);
    return (ix & ~meta->shift_mask) / meta->nodes_in_region;
}

inline uint64_t
AtomicDict_ShiftInRegionOf(uint64_t ix, atomic_dict_meta *meta)
{
    return ix & meta->shift_mask;
}

inline uint8_t *
AtomicDict_IndexAddressOf(uint64_t ix, atomic_dict_meta *meta)
{
    uint64_t shift = AtomicDict_ShiftInRegionOf(ix, meta);
    uint64_t region = AtomicDict_RegionOf(ix, meta);
    return (uint8_t *) &meta->index[region] + shift * (meta->node_size / 8);
}

inline int
AtomicDict_IndexAddressIsAligned(uint64_t ix, int alignment, atomic_dict_meta *meta)
{
    return (int64_t) AtomicDict_IndexAddressOf(ix, meta) % alignment == 0;
}

inline void
AtomicDict_ParseNodeFromRaw(uint64_t node_raw, atomic_dict_node *node,
                            atomic_dict_meta *meta)
{
    node->node = node_raw;
    node->index = (node_raw & meta->index_mask) >> (meta->node_size - meta->log_size);
    node->distance = (~(node_raw & meta->distance_mask) & meta->distance_mask) >> meta->tag_size;
    node->tag = node_raw & meta->tag_mask;
}


inline void
AtomicDict_ParseNodeFromRegion(uint64_t ix, uint64_t region, atomic_dict_node *node,
                               atomic_dict_meta *meta)
{
    uint64_t shift = AtomicDict_ShiftInRegionOf(ix, meta);
    uint64_t node_raw =
        (region & (meta->node_mask << (shift * meta->node_size))) >> (shift * meta->node_size);
    AtomicDict_ParseNodeFromRaw(node_raw, node, meta);
}

void
AtomicDict_ReadNodeAt(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta)
{
    uint64_t node_region = meta->index[AtomicDict_RegionOf(ix, meta)];
    AtomicDict_ParseNodeFromRegion(ix, node_region, node, meta);
}

/**
 * the following functions read more than one node at a time.
 * NB: they expect all the nodes to be in two successive words of memory!
 * this means that in order to call e.g. AtomicDict_Read8NodesAt,
 * meta->node_size must be at most 16.
 **/

void
AtomicDict_Read1NodeAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    uint64_t node_region = meta->index[AtomicDict_RegionOf(ix, meta)];
    AtomicDict_ParseNodeFromRegion(ix, node_region, &nodes[0], meta);
}

void
AtomicDict_Read2NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t big = AtomicDict_RegionOf(ix + 1, meta);
    uint64_t node_region_little = meta->index[little];
    uint64_t node_region_big = node_region_little;
    if (big != little) {
        node_region_big = meta->index[big];
    }
    AtomicDict_ParseNodeFromRegion(ix, node_region_little, &nodes[0], meta);
    AtomicDict_ParseNodeFromRegion(ix + 1, node_region_big, &nodes[1], meta);
}

void
AtomicDict_Read4NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size <= 32);
    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t middle = AtomicDict_RegionOf(ix + 2, meta);
    uint64_t big = AtomicDict_RegionOf(ix + 3, meta);
    uint64_t node_region_little = meta->index[little];
    uint64_t node_region_middle = node_region_little;
    uint64_t node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little) {
        node_region_big = meta->index[big];
    }

    uint64_t region;
    for (int i = 0; i < 4; ++i) {
        uint64_t r = AtomicDict_RegionOf(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        AtomicDict_ParseNodeFromRegion(ix + i, region, &nodes[i], meta);
    }
}

void
AtomicDict_Read8NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size <= 16);
    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t middle = AtomicDict_RegionOf(ix + 4, meta);
    uint64_t big = AtomicDict_RegionOf(ix + 7, meta);
    uint64_t node_region_little = meta->index[little];
    uint64_t node_region_middle = node_region_little;
    uint64_t node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little) {
        node_region_big = meta->index[big];
    }

    uint64_t region;
    for (int i = 0; i < 8; ++i) {
        uint64_t r = AtomicDict_RegionOf(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        AtomicDict_ParseNodeFromRegion(ix + i, region, &nodes[i], meta);
    }
}

void
AtomicDict_Read16NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta)
{
    assert(meta->node_size == 8);
    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t middle = AtomicDict_RegionOf(ix + 8, meta);
    uint64_t big = AtomicDict_RegionOf(ix + 15, meta);
    uint64_t node_region_little = meta->index[little];
    uint64_t node_region_middle = node_region_little;
    uint64_t node_region_big = node_region_little;
    if (middle != little) {
        node_region_middle = meta->index[middle];
    }
    if (big != little && big != middle) {
        node_region_big = meta->index[big];
    }

    uint64_t region;
    for (int i = 0; i < 16; ++i) {
        uint64_t r = AtomicDict_RegionOf(ix + i, meta);
        if (r == little) {
            region = node_region_little;
        } else if (r == middle) {
            region = node_region_middle;
        } else {
            region = node_region_big;
        }
        AtomicDict_ParseNodeFromRegion(ix + i, region, &nodes[i], meta);
    }
}


int
AtomicDict_WriteNodeAt(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta)
{
    AtomicDict_ComputeRawNode(node, meta);
    uint64_t shift = ix & meta->shift_mask;
    uint64_t region = AtomicDict_RegionOf(ix, meta);
    uint64_t node_raw = meta->index[region];
    node_raw &= ~(meta->node_mask << (shift * meta->node_size));
    node_raw |= node->node << (shift * meta->node_size);
    meta->index[region] = node_raw;
}

inline int
AtomicDict_MustWriteBytes(int n, atomic_dict_meta *meta)
{
    int n_bytes = (meta->node_size >> 3) * n;
    assert(n_bytes <= 16);
    if (n_bytes == 1) {
        return 1;
    }
    if (n_bytes <= 2) {
        return 2;
    }
    if (n_bytes <= 4) {
        return 4;
    }
    if (n_bytes <= 8) {
        return 8;
    }
    if (n_bytes <= 16) {
        return 16;
    }
    assert(0);
}

int
AtomicDict_AtomicWriteNodesAt(uint64_t ix, int n, atomic_dict_node *expected, atomic_dict_node *new,
                              atomic_dict_meta *meta)
{
    assert(n > 0);
    assert(n <= meta->nodes_in_two_regions);


    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t middle = AtomicDict_RegionOf(ix + n / 2, meta);
    uint64_t big = AtomicDict_RegionOf(ix + n - 1, meta);
    assert(little <= middle <= little + 1); // XXX implement index circular behavior
    assert(middle <= big <= middle + 1); // XXX implement index circular behavior
    __uint128_t expected_raw = 0, new_raw = 0, node; // il bello sta nelle piccole cose
    int i;
    for (i = 0; i < n; ++i) {
        AtomicDict_ComputeRawNode(&expected[i], meta);
        AtomicDict_ComputeRawNode(&new[i], meta);
    }
    for (i = 0; i < n; ++i) {
        node = expected[i].node;
        node <<= meta->node_size * i;
        expected_raw |= node;
        node = new[i].node;
        node <<= meta->node_size * i;
        new_raw |= node;
    }

    int must_write = AtomicDict_MustWriteBytes(n, meta);
    int must_write_nodes = must_write / (meta->node_size / 8);
    for (; i < must_write_nodes; ++i) {
        node = expected[i].node;
        node <<= meta->node_size * (meta->node_size - i - 1);
        new_raw |= node;
    }

    uint8_t *index_address = AtomicDict_IndexAddressOf(ix, meta);
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
            // assert memory access is aligned
            // this is not required for <=8 bytes CAS on x86_64
            assert(AtomicDict_IndexAddressIsAligned(ix, 16, meta));
            return __sync_bool_compare_and_swap_16(index_address, expected_raw, new_raw);
        default:
            assert(0);
    }
}
