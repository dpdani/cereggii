// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"


/**
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

    uint64_t unmasked_hash = 0;
    if (current_meta->log_size < new_meta->log_size) {
        unmasked_hash = current_meta->size;
    }

    uint64_t nodes_left = 0, nodes_right = 0;

    AtomicDict_BufferedNodeReader reader, reader_rx;
    reader.zone = reader_rx.zone = -1;

    AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, probe_length, &reader, &reader_rx,
                                              AtomicDict_RobinHoodCompact_IsNotTombstone);

    reader.zone = reader_rx.zone = -1;

    for (uint64_t i = probe_head; i < probe_head + probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (!AtomicDict_NodeIsTombstone(&reader.node, new_meta))
            continue;

        AtomicDict_WriteRawNodeAt(i, 0, new_meta);
    }

    reader.zone = reader_rx.zone = -1;

    for (uint64_t i = probe_head; i < probe_head + probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (reader.node.node == 0)
            break;

        int64_t block = (int64_t) AtomicDict_BlockOf(reader.node.index);

        if (current_meta->log_size == new_meta->log_size) {
            if (current_meta->greatest_refilled_block < block && block <= current_meta->greatest_deleted_block)
                continue;  // tombstone

            if (current_meta->greatest_deleted_block > -1 && block > current_meta->greatest_refilled_block) {
                int64_t new_block = block - current_meta->greatest_deleted_block;
                if (current_meta->greatest_refilled_block > -1) {
                    new_block += current_meta->greatest_refilled_block;
                }
                assert(current_meta->blocks[block] == new_meta->blocks[new_block]);
                reader.node.index = AtomicDict_PositionInBlockOf(reader.node.index) + (
                    new_block << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK
                );

                AtomicDict_WriteNodeAt(i, &reader.node, new_meta);
            }
        }

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(reader.node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.hash & unmasked_hash) {
            nodes_right++;
        } else {
            nodes_left++;
        }
    }

    if (nodes_left + nodes_right == 0)
        return ok;

    if (current_meta->log_size < new_meta->log_size) {
        AtomicDict_RobinHoodCompact_LeftRightSort(current_meta, new_meta, probe_head, nodes_left + nodes_right, &reader,
                                                  &reader_rx, AtomicDict_RobinHoodCompact_ShouldGoLeft);
        reader.zone = -1;

        for (uint64_t i = 0; i < nodes_right; ++i) {
            uint64_t idx = i + probe_head + nodes_left;
            AtomicDict_ReadNodesFromZoneStartIntoBuffer(idx, &reader, new_meta);

            AtomicDict_WriteRawNodeAt(idx, new_meta->zero.node, new_meta);
            AtomicDict_WriteNodeAt(current_meta->size + probe_head + i, &reader.node, new_meta);
        }

        int compacted = AtomicDict_RobinHoodCompact_CompactNodes(current_meta->size + probe_head, probe_length, 1,
                                                                 current_meta, new_meta);
        if (compacted < 0)
            goto fail;
    }

    int compacted = AtomicDict_RobinHoodCompact_CompactNodes(probe_head, probe_length, 0, current_meta, new_meta);

    if (compacted < 0)
        goto fail;

    return ok;
    fail:
    return failed;
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
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, reader_lx, reader_rx);
            AtomicDict_RobinHoodCompact_CompactNodes_Swap(left, right, reader_lx, reader_rx, new_meta,
                                                          NULL, probe_head);

            left++;
            right--;
            AtomicDict_RobinHoodCompact_ReadLeftRight(new_meta, left, right, &left_entry, &right_entry, &left_entry_p,
                                                      &right_entry_p, reader_lx, reader_rx);
        }
    }
}

inline void
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
int
AtomicDict_RobinHoodCompact_CompactNodes(uint64_t probe_head, uint64_t probe_length, int right,
                                         AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta)
{
    Py_hash_t hash_mask = (Py_hash_t) new_meta->size - 1;
    if (right) {
        hash_mask = (Py_hash_t) current_meta->size - 1;
    }

    uint64_t actual_probe_length;
    AtomicDict_BufferedNodeReader reader, reader_rx;
    reader.zone = reader_rx.zone = -1;

    for (actual_probe_length = 0; actual_probe_length < probe_length; ++actual_probe_length) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(probe_head + actual_probe_length, &reader, new_meta);

        if (reader.node.node == 0)
            break;
    }

    if (actual_probe_length == 0)
        return 0;

    int sorted = AtomicDict_RobinHoodCompact_CompactNodes_Sort(probe_head, actual_probe_length,
                                                               &reader, new_meta, hash_mask);
    if (sorted < 0)
        goto fail;

    reader.zone = reader_rx.zone = -1;

    // compute updated distances
    for (uint64_t i = probe_head + probe_length - 1; i >= probe_head; --i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(i, &reader, new_meta);

        if (reader.node.node == 0)
            continue;

        AtomicDict_Node node = reader.node;

        AtomicDict_Entry *entry_p, entry;
        entry_p = AtomicDict_GetEntryAt(node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        if (entry.key == NULL || entry.value == NULL || entry.flags & ENTRY_FLAGS_TOMBSTONE ||
            entry.flags & ENTRY_FLAGS_SWAPPED)
            continue;

        uint64_t distance_0 = AtomicDict_Distance0Of(entry.hash, new_meta);
        assert(distance_0 >= probe_head);
        assert(distance_0 <= probe_head + probe_length);
        if (i < distance_0) {
            AtomicDict_ReadNodesFromZoneStartIntoBuffer(distance_0, &reader, new_meta);
            assert(reader.node.node == 0);
            node.distance = 0;
            AtomicDict_WriteNodeAt(distance_0, &node, new_meta);
            AtomicDict_WriteRawNodeAt(i, new_meta->zero.node, new_meta);
            reader.zone = -1;
        } else {
            uint64_t distance = i - distance_0;

            if (distance > new_meta->max_distance) { // node will stay a reservation
                distance = new_meta->max_distance;
            }

            if (node.distance != distance) {
                node.distance = distance;
                AtomicDict_WriteNodeAt(i, &node, new_meta);
                reader.zone = -1;
            }
        }

        if (i == 0)
            break;
    }

    return 1;
    fail:
    return -1;
}


void
AtomicDict_RobinHoodCompact_CompactNodes_Swap(uint64_t ix_a, uint64_t ix_b, AtomicDict_BufferedNodeReader *reader_a,
                                              AtomicDict_BufferedNodeReader *reader_b, AtomicDict_Meta *new_meta,
                                              uint64_t *distance_0_cache, uint64_t probe_head)
{
    AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix_a, reader_a, new_meta);
    AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix_b, reader_b, new_meta);

    AtomicDict_WriteRawNodeAt(ix_a, reader_b->node.node, new_meta);
    AtomicDict_WriteRawNodeAt(ix_b, reader_a->node.node, new_meta);

    reader_a->zone = reader_b->zone = -1;
//    uint64_t raw_a = reader_a->node.node;
//    uint64_t raw_b = reader_b->node.node;
//    AtomicDict_ParseNodeFromRaw(raw_a, &reader_a->buffer[reader_a->idx_in_buffer], new_meta);
//    AtomicDict_ParseNodeFromRaw(raw_b, &reader_b->buffer[reader_b->idx_in_buffer], new_meta);
//    AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix_a, reader_a, new_meta);
//    AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix_b, reader_b, new_meta);

    if (distance_0_cache != NULL) {
        uint64_t tmp_dist = distance_0_cache[ix_b - probe_head];
        distance_0_cache[ix_b - probe_head] = distance_0_cache[ix_a - probe_head];
        distance_0_cache[ix_a - probe_head] = tmp_dist;
    }
}

int
AtomicDict_RobinHoodCompact_CompactNodes_Sort(uint64_t probe_head, uint64_t probe_length,
                                              AtomicDict_BufferedNodeReader *reader, AtomicDict_Meta *new_meta,
                                              Py_hash_t hash_mask)
{
    // timsort

    if (probe_length == 1)
        return 0;
    assert(probe_length < PY_SSIZE_T_MAX);

    PyObject *list = NULL;
    list = PyList_New((Py_ssize_t) probe_length);
    if (list == NULL)
        goto fail;

    PyObject *tuple = NULL, *borrowed_tuple = NULL, *py_raw_node = NULL;
    AtomicDict_Entry *entry_p, entry;

    for (uint64_t i = 0; i < probe_length; ++i) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(probe_head + i, reader, new_meta);
        entry_p = AtomicDict_GetEntryAt(reader->node.index, new_meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        uint64_t distance_0 = AtomicDict_Distance0Of(entry.hash & hash_mask, new_meta);

        tuple = Py_BuildValue("(kbk)", distance_0, reader->node.distance, reader->node.node);
        if (tuple == NULL)
            goto fail;

        assert(i < PY_SSIZE_T_MAX);
        PyList_SetItem(list, (Py_ssize_t) i, tuple);

        tuple = NULL;
    }

    int sorted = PyList_Sort(list);
    if (sorted < 0)
        goto fail;

    for (uint64_t i = 0; i < probe_length; ++i) {
        borrowed_tuple = PyList_GetItem(list, (Py_ssize_t) i);
        if (borrowed_tuple == NULL)
            goto fail;

        py_raw_node = PyTuple_GetItem(borrowed_tuple, 2);
        if (py_raw_node == NULL)
            goto fail;

        int overflow;
        uint64_t raw_node = PyLong_AsLongAndOverflow(py_raw_node, &overflow);

        if (overflow != 0 || PyErr_Occurred() != NULL)
            goto fail;

        AtomicDict_WriteRawNodeAt(probe_head + i, raw_node, new_meta);

        tuple = NULL;
        py_raw_node = NULL;
    }

    return 1;
    fail:
    Py_XDECREF(list);
    Py_XDECREF(tuple);
    return -1;
}
