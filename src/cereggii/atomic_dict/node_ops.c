// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"
#include "atomic_ops.h"

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.


inline void
AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    assert(node->index < meta->size);
    assert(node->distance <= meta->max_distance);
    node->node =
        (node->index << (meta->node_size - meta->log_size))
        | (meta->distance_mask - ((uint64_t) node->distance << (uint64_t) meta->tag_size))
        | (node->tag & meta->tag_mask);
}

inline int
AtomicDict_NodeIsReservation(AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    return node->node != 0 && node->distance == (1 << meta->distance_size) - 1;
}

inline int
AtomicDict_NodeIsTombstone(AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    return node->node == meta->tombstone.node;
}

inline uint64_t
AtomicDict_RegionOf(uint64_t ix, AtomicDict_Meta *meta)
{
    ix = ix % meta->size;
    return (ix & ~meta->shift_mask) / meta->nodes_in_region;
}

inline uint64_t
AtomicDict_ZoneOf(uint64_t ix, AtomicDict_Meta *meta)
{
    return AtomicDict_RegionOf(ix, meta) & (ULONG_MAX - 1UL);
}

inline uint64_t
AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta)
{
    return hash & (meta->size - 1);
}

inline uint64_t
AtomicDict_ShiftInRegionOf(uint64_t ix, AtomicDict_Meta *meta)
{
    return ix & meta->shift_mask;
}

inline uint8_t *
AtomicDict_IndexAddressOf(uint64_t ix, AtomicDict_Meta *meta)
{
    uint64_t shift = AtomicDict_ShiftInRegionOf(ix, meta);
    uint64_t region = AtomicDict_RegionOf(ix, meta);
    return (uint8_t *) &meta->index[region] + shift * (meta->node_size / 8);
}

inline int
AtomicDict_IndexAddressIsAligned(uint64_t ix, int alignment, AtomicDict_Meta *meta)
{
    return (int64_t) AtomicDict_IndexAddressOf(ix, meta) % alignment == 0;
}

inline void
AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                            AtomicDict_Meta *meta)
{
    node->node = node_raw;
    node->index = (node_raw & meta->index_mask) >> (meta->node_size - meta->log_size);
    node->distance = (~(node_raw & meta->distance_mask) & meta->distance_mask) >> meta->tag_size;
    node->tag = node_raw & meta->tag_mask;
}


inline void
AtomicDict_ParseNodeFromRegion(uint64_t ix, uint64_t region, AtomicDict_Node *node,
                               AtomicDict_Meta *meta)
{
    uint64_t shift = AtomicDict_ShiftInRegionOf(ix, meta);
    uint64_t node_raw =
        (region & (meta->node_mask << (shift * meta->node_size))) >> (shift * meta->node_size);
    AtomicDict_ParseNodeFromRaw(node_raw, node, meta);
}

inline void
AtomicDict_CopyNodeBuffers(AtomicDict_Node *from_buffer, AtomicDict_Node *to_buffer)
{
    for (int i = 0; i < 16; ++i) {
        to_buffer[i] = from_buffer[i];
    }
}

void
AtomicDict_ComputeBeginEndWrite(AtomicDict_Meta *meta, AtomicDict_Node *read_buffer, AtomicDict_Node *temp,
                                int *begin_write, int *end_write, int64_t *start_ix)
{
    int j;
    *begin_write = -1;
    for (j = 0; j < meta->nodes_in_zone; ++j) {
        AtomicDict_ComputeRawNode(&temp[j], meta);
        if (temp[j].node != read_buffer[j].node) {
            *begin_write = j;
            break;
        }
    }
    assert(*begin_write != -1);
    *end_write = -1;
    for (j = *begin_write + 1; j < meta->nodes_in_zone; ++j) {
        AtomicDict_ComputeRawNode(&temp[j], meta);
        if (temp[j].node == read_buffer[j].node) {
            *end_write = j;
            break;
        }
    }
    if (*end_write == -1) {
        *end_write = meta->nodes_in_zone;
    }
    assert(*end_write > *begin_write);

    if (AtomicDict_MustWriteBytes(*end_write - *begin_write, meta) == 16) {
        while (!AtomicDict_IndexAddressIsAligned(*start_ix + *begin_write, 16, meta)) {
            --*begin_write;
        }
        if (begin_write < 0) {
            *start_ix += *begin_write;
        }
    }
}

void
AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
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
AtomicDict_Read1NodeAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta)
{
    uint64_t node_region = meta->index[AtomicDict_RegionOf(ix, meta)];
    AtomicDict_ParseNodeFromRegion(ix, node_region, &nodes[0], meta);
}

void
AtomicDict_Read2NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta)
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
AtomicDict_Read4NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta)
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
AtomicDict_Read8NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta)
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
AtomicDict_Read16NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta)
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

inline void
AtomicDict_ReadNodesFromZoneIntoBuffer(uint64_t idx, AtomicDict_BufferedNodeReader *reader, AtomicDict_Meta *meta)
{
    idx &= (int64_t) (meta->size - 1);

    if (reader->zone != AtomicDict_ZoneOf(idx, meta)) {
        meta->read_nodes_in_zone(idx, reader->buffer, meta);
        reader->zone = (int64_t) AtomicDict_ZoneOf(idx, meta);
        reader->nodes_offset = (int) -(idx % meta->nodes_in_zone);
    }

    reader->idx_in_buffer = (int) (idx % meta->nodes_in_zone + reader->nodes_offset);
    assert(reader->idx_in_buffer < meta->nodes_in_zone);
    reader->node = reader->buffer[reader->idx_in_buffer];
}

inline void
AtomicDict_ReadNodesFromZoneStartIntoBuffer(uint64_t idx, AtomicDict_BufferedNodeReader *reader,
                                            AtomicDict_Meta *meta)
{
    AtomicDict_ReadNodesFromZoneIntoBuffer(idx / meta->nodes_in_zone * meta->nodes_in_zone,
                                           reader, meta);
    reader->idx_in_buffer = (int) (idx % meta->nodes_in_zone + reader->nodes_offset);
    reader->node = reader->buffer[reader->idx_in_buffer];
}


inline int
AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    AtomicDict_ComputeRawNode(node, meta);
    AtomicDict_WriteRawNodeAt(ix, node->node, meta);
}

inline int
AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta)
{
    uint64_t shift = ix & meta->shift_mask;
    uint64_t region = AtomicDict_RegionOf(ix, meta);
    uint64_t node_raw = meta->index[region];
    node_raw &= ~(meta->node_mask << (shift * meta->node_size));
    node_raw |= raw_node << (shift * meta->node_size);
    meta->index[region] = node_raw;
}

inline int
AtomicDict_MustWriteBytes(int n, AtomicDict_Meta *meta)
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
    return 16;
}

int
AtomicDict_AtomicWriteNodesAt(uint64_t ix, int n, AtomicDict_Node *expected, AtomicDict_Node *desired,
                              AtomicDict_Meta *meta)
{
    assert(n > 0);
    assert(n <= meta->nodes_in_zone);

    uint64_t little = AtomicDict_RegionOf(ix, meta);
    uint64_t middle = AtomicDict_RegionOf(ix + n / 2, meta);
    uint64_t big = AtomicDict_RegionOf(ix + n - 1, meta);
    assert(little <= middle <= little + 1); // XXX implement index circular behavior
    assert(middle <= big <= middle + 1); // XXX implement index circular behavior
    __uint128_t expected_raw = 0, desired_raw = 0, node; // il bello sta nelle piccole cose
    int i;
    for (i = 0; i < n; ++i) {
        AtomicDict_ComputeRawNode(&expected[i], meta);
        AtomicDict_ComputeRawNode(&desired[i], meta);
    }
    for (i = 0; i < n; ++i) {
        node = expected[i].node;
        node <<= meta->node_size * i;
        expected_raw |= node;
        node = desired[i].node;
        node <<= meta->node_size * i;
        desired_raw |= node;
    }

    int must_write = AtomicDict_MustWriteBytes(n, meta);
    int must_write_nodes = must_write / (meta->node_size / 8);
    for (; i < must_write_nodes; ++i) {
        node = expected[i].node;
        node <<= meta->node_size * (meta->node_size - i - 1);
        desired_raw |= node;
    }

    uint8_t *index_address = AtomicDict_IndexAddressOf(ix, meta);
    switch (must_write) {
        case 1:
            return CereggiiAtomic_CompareExchangeUInt8(index_address, expected_raw, desired_raw);
        case 2:
            return CereggiiAtomic_CompareExchangeUInt16((uint16_t *) index_address, expected_raw, desired_raw);
        case 4:
            return CereggiiAtomic_CompareExchangeUInt32((uint32_t *) index_address, expected_raw, desired_raw);
        case 8:
            return CereggiiAtomic_CompareExchangeUInt64((uint64_t *) index_address, expected_raw, desired_raw);
        case 16:
            // assert memory access is aligned
            // this is not required for <=8 bytes CAS on x86_64
            assert(AtomicDict_IndexAddressIsAligned(ix, 16, meta));
            return CereggiiAtomic_CompareExchangeUInt128((__uint128_t *) index_address, expected_raw, desired_raw);
        default:
            assert(0);
    }
}
