// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"


/*
 * Lythe and listin, gentilmen,
 * That be of frebore blode;
 * I shall you tel of a gode yeman,
 * His name was Robyn Hode.
 * */


AtomicDict_RobinHoodResult
AtomicDict_RobinHoodInsert(AtomicDict_Meta *meta, AtomicDict_Node *nodes, AtomicDict_Node *to_insert,
                           int distance_0_ix)
{
    /*
     * assumptions:
     *   1. there are meta->nodes_in_two_regions valid nodes in `nodes`
     *   2. there is at least 1 empty node
     * */

    AtomicDict_Node current = *to_insert;
    AtomicDict_Node temp;
    int probe = 0;
    int cursor;

    beginning:
    for (; probe < meta->nodes_in_zone; probe++) {
        if (probe >= meta->max_distance) {
            return grow;
        }
        cursor = distance_0_ix + probe;
        if (nodes[cursor].node == 0) {
            if (!AtomicDict_NodeIsReservation(&current, meta)) {
                current.distance = probe;
            }
            nodes[cursor] = current;
            return ok;
        }

        if (nodes[cursor].distance < probe) {
            if (!AtomicDict_NodeIsReservation(&current, meta)) {
                current.distance = probe;
                AtomicDict_ComputeRawNode(&current, meta);
            }
            temp = nodes[cursor];
            nodes[cursor] = current;
            current = temp;
            distance_0_ix = cursor - temp.distance;
            probe = temp.distance;
            goto beginning;
        }
    }
    return failed;
}

AtomicDict_RobinHoodResult
AtomicDict_RobinHoodDelete(AtomicDict_Meta *meta, AtomicDict_Node *nodes, int to_delete)
{
    assert(to_delete >= 0);
    assert(to_delete < meta->nodes_in_zone);

    AtomicDict_Node temp;

    nodes[to_delete] = meta->tombstone;
    int probe;

    for (probe = to_delete; probe + 1 < meta->nodes_in_zone &&
                            nodes[probe + 1].node != 0 &&
                            !AtomicDict_NodeIsReservation(&nodes[probe + 1], meta) &&
                            nodes[probe + 1].distance > 0; probe++) {
        temp = nodes[probe];
        nodes[probe] = nodes[probe + 1];
        nodes[probe + 1] = temp;
        nodes[probe].distance--;
    }

    if (probe + 1 < meta->nodes_in_zone && (
        nodes[probe + 1].node == 0 || nodes[probe + 1].distance == 0
    )) {
        AtomicDict_ParseNodeFromRaw(0, &nodes[probe], meta);
    }

    return ok;
}

AtomicDict_RobinHoodResult
AtomicDict_RobinHoodCompact(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta, uint64_t probe_head,
                            uint64_t probe_length)
{
    assert(probe_length >= 2);
    assert(current_meta->log_size + 1 == new_meta->log_size);

    uint64_t unmasked_hash = current_meta->size;

    uint64_t nodes_left = 0, nodes_right = 0;

    AtomicDict_BufferedNodeReader reader, reader_rx;
    reader.zone = reader_rx.zone = -1;

    AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, probe_length, &reader, &reader_rx,
                                              AtomicDict_RobinHoodCompact_IsNotTombstone);

    reader.zone = reader_rx.zone = -1;

    for (uint64_t i = probe_head; i < probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (!AtomicDict_NodeIsTombstone(&reader.node, new_meta))
            continue;

        AtomicDict_WriteRawNodeAt(i, 0, new_meta);
    }

    reader.zone = reader_rx.zone = -1;

    for (uint64_t i = probe_head; i < probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (reader.node.node == 0)
            break;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(reader.node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.hash & unmasked_hash) {
            nodes_right++;
        } else {
            nodes_left++;
        }
    }

    AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, nodes_left + nodes_right, &reader,
                                              &reader_rx,
                                              AtomicDict_RobinHoodCompact_ShouldGoLeft);

    reader.zone = -1;

    for (uint64_t i = 0; i < nodes_right; ++i) {
        uint64_t idx = i + probe_head + nodes_left;
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(idx, &reader, new_meta);

        AtomicDict_WriteRawNodeAt(idx, new_meta->zero.node, new_meta);
        AtomicDict_WriteNodeAt(current_meta->size + probe_head + i, &reader.node, new_meta);
    }

    AtomicDict_RobinHoodCompact_CompactNodes(probe_head, probe_length, 0, current_meta, new_meta);
    AtomicDict_RobinHoodCompact_CompactNodes(current_meta->size + probe_head, probe_length, 1, current_meta, new_meta);

    return ok;
}

void
AtomicDict_RobinHoodCompact_LeftRightSort(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta,
                                          uint64_t probe_head, uint64_t probe_length,
                                          AtomicDict_BufferedNodeReader *reader_lx,
                                          AtomicDict_BufferedNodeReader *reader_rx,
                                          int (*should_go_left)(AtomicDict_Node *node, AtomicDict_Entry *entry,
                                                                AtomicDict_Meta *current_meta,
                                                                AtomicDict_Meta *new_meta))
{
    // basically a quicksort partition

    uint64_t left = probe_head;
    assert(probe_length > 0);
    uint64_t right = probe_length - 1;
    AtomicDict_Entry left_entry, right_entry;
    AtomicDict_Entry *left_entry_p, *right_entry_p;

    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                              &right_entry_p, reader_lx, reader_rx);

    while (left < right) {
        // move 'left' index to the right until an element that should go to the right is found
        while (left < right && should_go_left(&reader_lx->node, &left_entry, current_meta, new_meta)) {
            left++;
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, reader_lx, reader_rx);
        }

        // move 'right' index to the left until an element that should go to the left is found
        while (left < right && !should_go_left(&reader_rx->node, &right_entry, current_meta, new_meta)) {
            right--;
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, reader_lx, reader_rx);
        }

        // swap elements
        if (left < right) {
            AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, right, &reader_lx->node, &reader_rx->node, new_meta);

            left++;
            right--;
        }
    }
}

void
AtomicDict_RobinHoodCompact_ReadLeftRight(AtomicDict_Meta *new_meta, uint64_t left, uint64_t right,
                                          AtomicDict_Entry *left_entry, AtomicDict_Entry *right_entry,
                                          AtomicDict_Entry **left_entry_p, AtomicDict_Entry **right_entry_p,
                                          AtomicDict_BufferedNodeReader *reader_lx,
                                          AtomicDict_BufferedNodeReader *reader_rx)
{
    left %= new_meta->size;
    right %= new_meta->size;
    AtomicDict_ReadNodesFromZoneStartIntoBuffer(left, reader_lx, new_meta);
    AtomicDict_ReadNodesFromZoneStartIntoBuffer(right, reader_rx, new_meta);
    *left_entry_p = AtomicDict_GetEntryAt(reader_lx->node.index, new_meta);
    AtomicDict_ReadEntry(*left_entry_p, left_entry);
    *right_entry_p = AtomicDict_GetEntryAt(reader_rx->node.index, new_meta);
    AtomicDict_ReadEntry(*right_entry_p, right_entry);
}

inline int
AtomicDict_RobinHoodCompact_ShouldGoLeft(AtomicDict_Node *Py_UNUSED(node), AtomicDict_Entry *entry,
                                         AtomicDict_Meta *current_meta, AtomicDict_Meta *Py_UNUSED(new_meta))
{
    return (entry->hash & current_meta->size) == 0;
}

inline int
AtomicDict_RobinHoodCompact_IsNotTombstone(AtomicDict_Node *node, AtomicDict_Entry *Py_UNUSED(entry),
                                           AtomicDict_Meta *Py_UNUSED(current_meta), AtomicDict_Meta *new_meta)
{
    return !AtomicDict_NodeIsTombstone(node, new_meta);
}

// here comes dat boi
void
AtomicDict_RobinHoodCompact_CompactNodes(uint64_t probe_head, uint64_t probe_length, int right,
                                         AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t hash_mask = new_meta->size - 1;
    if (right) {
        hash_mask = current_meta->size - 1;
    }

    uint64_t actual_probe_length;
    AtomicDict_BufferedNodeReader reader;
    reader.zone = -1;

    for (actual_probe_length = 0; actual_probe_length < probe_length; ++actual_probe_length) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(probe_head + actual_probe_length, &reader, new_meta);

        if (reader.node.node == 0)
            break;
    }

    if (actual_probe_length == 0)
        return;

    AtomicDict_RobinHoodCompact_CompactNodes_Sort(probe_head, actual_probe_length, hash_mask, current_meta, new_meta);

    // compute updated distances
    for (uint64_t i = probe_head + probe_length - 1; i >= probe_head; --i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (reader.node.node == 0)
            continue;

        AtomicDict_Node node = reader.node;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        uint64_t distance_0 = AtomicDict_Distance0Of(entry.hash, new_meta);
        assert(distance_0 >= probe_head);
        assert(distance_0 <= probe_head + probe_length);
        if (i < distance_0) {
            AtomicDict_ReadNodesFromZoneStartIntoBuffer(distance_0, &reader, new_meta);
            assert(reader.node.node == 0);
            node.distance = 0;
            AtomicDict_WriteNodeAt(distance_0, &node, new_meta);
            AtomicDict_WriteRawNodeAt(i, new_meta->zero.node, new_meta);
        } else {
            uint64_t distance = i - distance_0;

            if (distance > new_meta->max_distance) { // node will stay a reservation
                distance = new_meta->max_distance;
            }

            if (node.distance != distance) {
                node.distance = distance;
                AtomicDict_WriteNodeAt(i, &node, new_meta);
            }
        }

        if (i == 0)
            break;
    }
}

void
AtomicDict_RobinHoodCompact_CompactNodes_Sort(uint64_t probe_head, uint64_t probe_length, uint64_t hash_mask,
                                              AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    // quicksort adapted for robin hood hashing

    AtomicDict_BufferedNodeReader reader_lx, reader_rx;
    reader_lx.zone = reader_rx.zone = -1;

    if (probe_head < probe_head + probe_length) {
        uint64_t pi = AtomicDict_RobinHoodCompact_CompactNodes_Partition(probe_head, probe_length, hash_mask,
                                                                         &reader_lx, &reader_rx, current_meta,
                                                                         new_meta);

        uint64_t left_head = probe_head;
        uint64_t left_length = 0;
        if (probe_head != pi) {
            left_length = (probe_head + pi - 1) % new_meta->size;
        }
        uint64_t right_head = (pi + 1) % new_meta->size;
        uint64_t right_length = 0;
        if (pi != probe_head + probe_length - 1) {
            right_length = (probe_head + probe_length - pi - 1) % new_meta->size;
        }
        AtomicDict_RobinHoodCompact_CompactNodes_Sort(left_head, left_length, hash_mask, current_meta, new_meta);
        AtomicDict_RobinHoodCompact_CompactNodes_Sort(right_head, right_length, hash_mask, current_meta, new_meta);
    }
}

uint64_t
AtomicDict_RobinHoodCompact_CompactNodes_Partition(uint64_t probe_head, uint64_t probe_length, uint64_t hash_mask,
                                                   AtomicDict_BufferedNodeReader *reader_lx,
                                                   AtomicDict_BufferedNodeReader *reader_rx,
                                                   AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t left = probe_head - 1;
    uint64_t right = probe_head + probe_length - 1;
    AtomicDict_Entry left_entry, right_entry;
    AtomicDict_Entry *left_entry_p, *right_entry_p;

    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                              &right_entry_p, reader_lx, reader_rx);

    uint64_t pivot = AtomicDict_Distance0Of(right_entry.hash & (Py_hash_t) hash_mask, new_meta);

    for (right = probe_head; right < probe_head + probe_length; right++) {
        AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left + 1, right, &left_entry,
                                                  &right_entry, &left_entry_p, &right_entry_p, reader_lx, reader_rx);
        uint64_t current = AtomicDict_Distance0Of(right_entry.hash & (Py_hash_t) hash_mask, new_meta);

        if (current < pivot) {
            left++;
            AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, right, &reader_lx->node, &reader_rx->node, new_meta);
        }
    }

    left++;
    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, probe_head + probe_length - 1, &left_entry,
                                              &right_entry, &left_entry_p, &right_entry_p,
                                              reader_lx, reader_rx);
    AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, probe_head + probe_length - 1, &reader_lx->node,
                                                  &reader_rx->node,
                                                  new_meta);
    return left;
}

void
AtomicDict_RobinHoodCompact_CompactNodes_Swap(uint64_t ix_a, uint64_t ix_b, AtomicDict_Node *a, AtomicDict_Node *b,
                                              AtomicDict_Meta *new_meta)
{
    AtomicDict_WriteNodeAt(ix_a, b, new_meta);
    AtomicDict_WriteNodeAt(ix_b, a, new_meta);
}
