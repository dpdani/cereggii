// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"


AtomicDict_RobinHoodResult
AtomicDict_RobinHoodInsert(AtomicDict_Meta *meta, AtomicDict_Node *nodes, AtomicDict_Node *to_insert,
                           int distance_0_ix)
{
    /*
     * Lythe and listin, gentilmen,
     * That be of frebore blode;
     * I shall you tel of a gode yeman,
     * His name was Robyn Hode.
     * */

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

    for (uint64_t i = probe_head; i < probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (AtomicDict_NodeIsTombstone(&reader.node, new_meta))
            continue;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(reader.node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.hash & unmasked_hash) {
            nodes_right++;
        } else {
            nodes_left++;
        }
    }

    AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, probe_length, &reader, &reader_rx,
                                              AtomicDict_RobinHoodCompact_IsNotTombstone);

    for (uint64_t i = nodes_left + nodes_right; i < probe_length; ++i) {
        AtomicDict_WriteRawNodeAt(i, 0, new_meta);
    }

    AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, nodes_left + nodes_right, &reader,
                                              &reader_rx,
                                              AtomicDict_RobinHoodCompact_ShouldGoLeft);

    AtomicDict_RobinHoodCompact_CompactNodes(probe_head, nodes_left, 0, current_meta, new_meta);
    AtomicDict_RobinHoodCompact_CompactNodes(probe_head + nodes_left, nodes_right, 1, current_meta, new_meta);

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
    uint64_t right = probe_length - 1;
    AtomicDict_Node left_node, right_node;
    AtomicDict_Entry left_entry, right_entry;
    AtomicDict_Entry *left_entry_p, *right_entry_p;

    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                              &right_entry_p, &left_node, &right_node, reader_lx, reader_rx);

    while (left < right) {
        // move 'left' index to the right until an element that should go to the right is found
        while (left < right && should_go_left(&left_node, &left_entry, current_meta, new_meta)) {
            left++;
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, &left_node, &right_node, reader_lx, reader_rx);
        }

        // move 'right' index to the left until an element that should go to the left is found
        while (left < right && !should_go_left(&right_node, &right_entry, current_meta, new_meta)) {
            right--;
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, &left_node, &right_node, reader_lx, reader_rx);
        }

        // swap elements
        if (left < right) {
            AtomicDict_WriteNodeAt(left, &right_node, new_meta);
            AtomicDict_WriteNodeAt(right, &left_node, new_meta);

            left++;
            right--;
        }
    }
}

void
AtomicDict_RobinHoodCompact_ReadLeftRight(AtomicDict_Meta *new_meta, uint64_t left, uint64_t right,
                                          AtomicDict_Entry *left_entry, AtomicDict_Entry *right_entry,
                                          AtomicDict_Entry **left_entry_p, AtomicDict_Entry **right_entry_p,
                                          AtomicDict_Node *left_node, AtomicDict_Node *right_node,
                                          AtomicDict_BufferedNodeReader *reader_lx,
                                          AtomicDict_BufferedNodeReader *reader_rx)
{
    AtomicDict_ReadNodesFromZoneIntoBuffer(left, reader_lx, new_meta);
    AtomicDict_ReadNodesFromZoneIntoBuffer(right, reader_rx, new_meta);
    *left_entry_p = AtomicDict_GetEntryAt(left_node->index, new_meta);
    AtomicDict_ReadEntry(*left_entry_p, left_entry);
    *right_entry_p = AtomicDict_GetEntryAt(right_node->index, new_meta);
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
    uint64_t unmasked_hash = 0;
    if (right) {
        unmasked_hash = current_meta->size;
    }
    unmasked_hash = ~unmasked_hash;

    AtomicDict_RobinHoodCompact_CompactNodes_Sort(probe_head, probe_length, unmasked_hash, current_meta, new_meta);

    AtomicDict_BufferedNodeReader reader;
    reader.zone = -1;

    // compute updated distances
    for (uint64_t i = probe_head; i < probe_head + probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneIntoBuffer(i, &reader, new_meta);

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(reader.node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        uint64_t distance_0 = AtomicDict_Distance0Of(entry.hash & (Py_hash_t) unmasked_hash, new_meta);
        assert(i >= distance_0);
        uint64_t distance = i - distance_0;

        if (distance > new_meta->max_distance) { // node will stay a reservation
            distance = new_meta->max_distance;
        }

        if (reader.node.distance != distance) {
            reader.node.distance = distance;
            AtomicDict_WriteNodeAt(i, &reader.node, new_meta);
        }
    }
}

void
AtomicDict_RobinHoodCompact_CompactNodes_Sort(uint64_t probe_head, uint64_t probe_length, uint64_t unmasked_hash,
                                              AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    // quicksort adapted for robin hood hashing

    AtomicDict_BufferedNodeReader reader_lx, reader_rx;
    reader_lx.zone = reader_rx.zone = -1;

    if (probe_head < probe_head + probe_length) {
        uint64_t pi = AtomicDict_RobinHoodCompact_CompactNodes_Partition(probe_head, probe_length, unmasked_hash,
                                                                         &reader_lx, &reader_rx, current_meta,
                                                                         new_meta);

        AtomicDict_RobinHoodCompact_CompactNodes_Sort(probe_head, probe_head + pi - 1, unmasked_hash, current_meta,
                                                      new_meta);
        AtomicDict_RobinHoodCompact_CompactNodes_Sort(pi + 1, probe_length - pi, unmasked_hash, current_meta, new_meta);
    }
}

uint64_t
AtomicDict_RobinHoodCompact_CompactNodes_Partition(uint64_t probe_head, uint64_t probe_length, uint64_t unmasked_hash,
                                                   AtomicDict_BufferedNodeReader *reader_lx,
                                                   AtomicDict_BufferedNodeReader *reader_rx,
                                                   AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    uint64_t left = probe_head - 1;
    uint64_t right = probe_head + probe_length - 1;
    AtomicDict_Node left_node, right_node;
    AtomicDict_Entry left_entry, right_entry;
    AtomicDict_Entry *left_entry_p, *right_entry_p;

    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                              &right_entry_p, &left_node, &right_node, reader_lx, reader_rx);

    uint64_t pivot = AtomicDict_Distance0Of(right_entry.hash & (Py_hash_t) unmasked_hash, new_meta);

    for (right = left; right < probe_head + probe_length; ++right) {
        AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left + 1, right, &left_entry, &right_entry, &left_entry_p,
                                                  &right_entry_p, &left_node, &right_node, reader_lx, reader_rx);
        uint64_t current = AtomicDict_Distance0Of(right_entry.hash & (Py_hash_t) unmasked_hash, new_meta);

        if (current < pivot) {
            left++;
            AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, right, &left_node, &right_node, new_meta);
        }
    }

    left++;
    AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, probe_head + probe_length - 1, &left_entry,
                                              &right_entry, &left_entry_p, &right_entry_p, &left_node, &right_node,
                                              reader_lx, reader_rx);
    AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, probe_head + probe_length - 1, &left_node, &right_node,
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
