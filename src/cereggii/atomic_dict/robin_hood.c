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
