// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"


typedef enum AtomicDict_RobinHoodResult {
    ok,
    failed,
    grow,
} AtomicDict_RobinHoodResult;

AtomicDict_RobinHoodResult
AtomicDict_RobinHood(atomic_dict_meta *meta, atomic_dict_node *nodes, atomic_dict_node *to_insert, int distance_0_ix)
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

    atomic_dict_node current = *to_insert;
    atomic_dict_node temp;
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


typedef enum AtomicDict_InsertedOrUpdated {
    error,
    inserted,
    updated,
    nop,
    retry,
} AtomicDict_InsertedOrUpdated;

AtomicDict_InsertedOrUpdated
AtomicDict_CheckNodeEntryAndMaybeUpdate(uint64_t distance_0, uint64_t i, atomic_dict_node *node,
                                        atomic_dict_meta *meta, Py_hash_t hash, PyObject *key, PyObject *value)
{
    if (AtomicDict_NodeIsReservation(node, meta))
        goto check_entry;
    if (meta->is_compact && (distance_0 + i - node->distance) % meta->size > distance_0)
        return nop;
    if (node->tag != (hash & meta->tag_mask))
        return nop;

    atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
    check_entry:
    entry_p = AtomicDict_GetEntryAt(node->index, meta);
    entry = *entry_p;

    if (entry.hash != hash)
        return nop;
    if (entry.key == key)
        goto same_key_reservation;

    int cmp = PyObject_RichCompareBool(entry.key, key, Py_EQ);
    if (cmp < 0) {
        // exception thrown during compare
        return error;
    }
    if (cmp == 0)
        return nop;

    same_key_reservation:
    if (entry.flags & ENTRY_FLAGS_TOMBSTONE) {
        // help delete
        return retry;
    }
    if (AtomicDict_NodeIsReservation(node, meta)) {
        // help insert
        return retry;
    }

    if (CereggiiAtomic_CompareExchangePtr((void **) &entry_p->value, entry.value, value)) {
        Py_DECREF(entry.value);
        return updated;
    }
    goto check_entry;
}

void
AtomicDict_ComputeBeginEndWrite(atomic_dict_meta *meta, atomic_dict_node *read_buffer, atomic_dict_node *temp,
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

AtomicDict_InsertedOrUpdated
AtomicDict_InsertOrUpdateCloseToDistance0(AtomicDict *self, atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc,
                                          Py_hash_t hash, PyObject *key, PyObject *value, uint64_t distance_0,
                                          atomic_dict_node *read_buffer, atomic_dict_node *temp, int64_t *zone,
                                          int *idx_in_buffer, int *nodes_offset)
{
    atomic_dict_node node = {
        .index = entry_loc->location,
        .tag = hash,
    }, _;

    beginning:
    AtomicDict_ReadNodesFromZoneIntoBuffer(distance_0, zone, read_buffer, &_, idx_in_buffer, nodes_offset, meta);
    AtomicDict_InsertedOrUpdated check_result;

    for (int i = 0; i < meta->nodes_in_zone; ++i) {
        if (read_buffer[i].node == 0) {
            AtomicDict_CopyNodeBuffers(read_buffer, temp);
            AtomicDict_RobinHoodResult rh = AtomicDict_RobinHood(meta, temp, &node, 0);
            if (rh == grow) {
                AtomicDict_Grow(self);
                return inserted;
            }
            assert(rh == ok);
            int begin_write, end_write;
            AtomicDict_ComputeBeginEndWrite(meta, read_buffer, temp, &begin_write, &end_write, (int64_t *) &distance_0);
            if (begin_write < 0)
                goto beginning;
            if (AtomicDict_AtomicWriteNodesAt(distance_0 + begin_write, end_write - begin_write,
                                              &read_buffer[begin_write], &temp[begin_write], meta)) {
                return inserted;
            }
            goto beginning;
        }

        check_result = AtomicDict_CheckNodeEntryAndMaybeUpdate(distance_0, i, &read_buffer[i], meta, hash, key, value);
        if (check_result == retry) {
            goto beginning;
        }
        if (check_result == error) {
            goto error;
        }
        if (check_result == updated) {
            return updated;
        }
    }

    return nop;

    error:
    return error;
}

int
AtomicDict_InsertOrUpdate(AtomicDict *self, atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc)
{
    Py_hash_t hash = entry_loc->entry->hash;
    PyObject *key = entry_loc->entry->key;
    PyObject *value = entry_loc->entry->value;
    assert(key != NULL && value != NULL);
    uint64_t distance_0 = AtomicDict_Distance0Of(hash, meta);
    assert(distance_0 >= 0);

    atomic_dict_node read_buffer[16];
    atomic_dict_node temp[16];
    atomic_dict_node node, reservation;
    uint64_t idx;
    int64_t zone = -1;
    int nodes_offset, idx_in_buffer;

    AtomicDict_InsertedOrUpdated close_to_0;
    close_to_0 = AtomicDict_InsertOrUpdateCloseToDistance0(self, meta, entry_loc, hash, key, value, distance_0,
                                                           read_buffer, temp, &zone, &idx_in_buffer, &nodes_offset);
    if (close_to_0 == error) {
        goto error;
    }
    if (close_to_0 == inserted || close_to_0 == updated) {
        return close_to_0;
    }

    beginning:
    for (int i = 0; i < meta->size; ++i) {
        idx = (distance_0 + i) % (meta->size);

        AtomicDict_ReadNodesFromZoneIntoBuffer(distance_0 + i, &zone, read_buffer, &node, &idx_in_buffer,
                                               &nodes_offset, meta);

        if (node.node == 0)
            goto tail_found;

        AtomicDict_InsertedOrUpdated check_entry;
        check_entry = AtomicDict_CheckNodeEntryAndMaybeUpdate(distance_0, i, &node, meta, hash, key, value);
        switch (check_entry) {
            case retry:
                goto beginning;
            case error:
                goto error;
            case updated:
                return updated;
            case nop:
                continue;
            default:
                assert(0);
        }
    }

    // looped over the entire index without finding an emtpy slot
    AtomicDict_Grow(self);
    return inserted; // linearization point is inside grow()

    tail_found:
    reservation.index = entry_loc->location;
    reservation.distance = meta->max_distance;
    reservation.tag = hash;
    assert(node.node == 0);
    if (meta->is_compact) {
        CereggiiAtomic_CompareExchangeUInt8(&meta->is_compact, 1, 0);
        // no need to handle failure
    }
    if (!AtomicDict_AtomicWriteNodesAt(idx, 1, &read_buffer[idx_in_buffer], &reservation, meta)) {
        zone = -1;
        goto beginning;
    }

    return inserted;
    error:
    return error;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    if (value == NULL) {
        return AtomicDict_DelItem(self, key);
    }

    assert(key != NULL);
    assert(value != NULL);

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1) {
        return -1;
    }

    atomic_dict_reservation_buffer *rb = AtomicDict_GetReservationBuffer(self);
    if (rb == NULL)
        goto fail;

    atomic_dict_meta *meta;
    meta = (atomic_dict_meta *) AtomicRef_Get(self->metadata);

    atomic_dict_entry_loc entry_loc;
    AtomicDict_GetEmptyEntry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL)
        goto fail;

    Py_INCREF(key);
    Py_INCREF(value);
    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;

    AtomicDict_InsertedOrUpdated result = AtomicDict_InsertOrUpdate(self, meta, &entry_loc);

    if (result != inserted) {
        entry_loc.entry->flags &= ENTRY_FLAGS_RESERVED; // keep reserved, or set to 0
        entry_loc.entry->key = 0;
        entry_loc.entry->value = 0;
        entry_loc.entry->hash = 0;
        AtomicDict_ReservationBufferPut(rb, &entry_loc, 1);
    }
    if (result == error) {
        goto fail;
    }

    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}
