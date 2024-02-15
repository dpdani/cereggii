// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"


inline AtomicDict_InsertedOrUpdated
AtomicDict_CheckNodeEntryAndMaybeUpdate(uint64_t distance_0, uint64_t i, AtomicDict_Node *node,
                                        AtomicDict_Meta *meta, Py_hash_t hash, PyObject *key, PyObject *value)
{
    if (AtomicDict_NodeIsReservation(node, meta))
        goto check_entry;
    if (meta->is_compact && (distance_0 + i - node->distance) % meta->size > distance_0)
        return nop;
    if (node->tag != (hash & meta->tag_mask))
        return nop;

    AtomicDict_Entry *entry_p;
    AtomicDict_Entry entry;
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

AtomicDict_InsertedOrUpdated
AtomicDict_InsertOrUpdateCloseToDistance0(AtomicDict_Meta *meta, AtomicDict_EntryLoc *entry_loc, Py_hash_t hash,
                                          PyObject *key, PyObject *value, uint64_t distance_0,
                                          AtomicDict_BufferedNodeReader *reader, AtomicDict_Node *temp)
{
    AtomicDict_Node node = {
        .index = entry_loc->location,
        .tag = hash,
    }, _;

    beginning:
    reader->zone = -1;
    AtomicDict_ReadNodesFromZoneIntoBuffer(distance_0, reader, meta);
    AtomicDict_InsertedOrUpdated check_result;

    for (int i = 0; i < meta->nodes_in_zone; ++i) {
        if (reader->buffer[i].node == 0) {
            AtomicDict_CopyNodeBuffers(reader->buffer, temp);
            AtomicDict_RobinHoodResult rh = AtomicDict_RobinHoodInsert(meta, temp, &node, reader->idx_in_buffer);

            if (rh == grow) {
                return must_grow;
            }

            assert(rh == ok);
            int begin_write, end_write;
            AtomicDict_ComputeBeginEndWrite(meta, reader->buffer, temp, &begin_write, &end_write,
                                            (int64_t *) &distance_0);
            if (begin_write < 0)
                goto beginning;
            if (AtomicDict_AtomicWriteNodesAt(distance_0 + begin_write, end_write - begin_write,
                                              &reader->buffer[begin_write], &temp[begin_write], meta)) {
                return inserted;
            }
            goto beginning;
        }

        check_result = AtomicDict_CheckNodeEntryAndMaybeUpdate(distance_0, i, &reader->buffer[i], meta, hash, key,
                                                               value);
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

AtomicDict_InsertedOrUpdated
AtomicDict_InsertOrUpdate(AtomicDict_Meta *meta, AtomicDict_EntryLoc *entry_loc)
{
    Py_hash_t hash = entry_loc->entry->hash;
    PyObject *key = entry_loc->entry->key;
    PyObject *value = entry_loc->entry->value;
    assert(key != NULL && value != NULL);
    uint64_t distance_0 = AtomicDict_Distance0Of(hash, meta);
    assert(distance_0 >= 0);

    AtomicDict_BufferedNodeReader reader;
    AtomicDict_Node temp[16];
    AtomicDict_Node reservation;
    uint64_t idx;

    AtomicDict_InsertedOrUpdated close_to_0;
    close_to_0 = AtomicDict_InsertOrUpdateCloseToDistance0(meta, entry_loc, hash, key, value, distance_0,
                                                           &reader, temp);
    if (close_to_0 == error) {
        goto error;
    }
    if (close_to_0 == inserted || close_to_0 == updated || close_to_0 == must_grow) {
        return close_to_0;
    }

    beginning:
    for (int i = 0; i < meta->size; ++i) {
        idx = (distance_0 + i) % (meta->size);

        AtomicDict_ReadNodesFromZoneIntoBuffer(distance_0 + i, &reader, meta);

        if (reader.node.node == 0)
            goto tail_found;

        AtomicDict_InsertedOrUpdated check_entry;
        check_entry = AtomicDict_CheckNodeEntryAndMaybeUpdate(distance_0, i, &reader.node, meta, hash, key, value);
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
    return must_grow;

    tail_found:
    reservation.index = entry_loc->location;
    reservation.distance = meta->max_distance;
    reservation.tag = hash;
    assert(reader.node.node == 0);
    if (meta->is_compact) {
        CereggiiAtomic_CompareExchangeUInt8(&meta->is_compact, 1, 0);
        // no need to handle failure
    }
    if (!AtomicDict_AtomicWriteNodesAt(idx, 1, &reader.buffer[reader.idx_in_buffer], &reservation, meta)) {
        reader.zone = -1;
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

    Py_INCREF(key);
    Py_INCREF(value);

    AtomicDict_Meta *meta = NULL;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_ReservationBuffer *rb = NULL;
    rb = AtomicDict_GetReservationBuffer(self);
    if (rb == NULL)
        goto fail;


    beginning:
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int migrated = AtomicDict_MaybeHelpMigrate(self, meta);
    if (migrated) {
        Py_DECREF(meta);
        meta = NULL;
        goto beginning;
    }

    AtomicDict_EntryLoc entry_loc;
    int got_entry = AtomicDict_GetEmptyEntry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL || got_entry == -1)
        goto fail;

    if (got_entry == 0) {  // => must grow
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        Py_DECREF(meta);
        meta = NULL;
        goto beginning;
    }

    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;

    AtomicDict_InsertedOrUpdated result = AtomicDict_InsertOrUpdate(meta, &entry_loc);

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

    if (result == must_grow) {
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        Py_DECREF(meta);
        goto beginning;
    }

    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}
