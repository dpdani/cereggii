// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"


void
AtomicDict_Lookup(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                  AtomicDict_SearchResult *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    uint64_t ix = AtomicDict_Distance0Of(hash, meta);
    uint8_t is_compact;
    uint64_t probe, reservations;
    AtomicDict_BufferedNodeReader reader;

    beginning:
    is_compact = meta->is_compact;
    reservations = 0;
    reader.zone = -1;

    for (probe = 0; probe < meta->size; probe++) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix + probe + reservations, &reader, meta);

        if (AtomicDict_NodeIsReservation(&reader.node, meta)) {
            probe--;
            reservations++;
            result->is_reservation = 1;
            goto check_entry;
        }
        result->is_reservation = 0;

        if (reader.node.node == 0) {
            goto not_found;
        }

        if (
            is_compact && (
                (ix + probe + reservations - reader.node.distance > ix)
                || (probe >= meta->log_size)
            )) {
            goto not_found;
        }

        if (reader.node.tag == (hash & meta->tag_mask)) {
            check_entry:
            result->entry_p = AtomicDict_GetEntryAt(reader.node.index, meta);
            AtomicDict_ReadEntry(result->entry_p, &result->entry);

            if (result->entry.value == NULL || result->entry.flags & ENTRY_FLAGS_TOMBSTONE) {
                continue;
            }
            if (result->entry.key == key) {
                goto found;
            }
            if (result->entry.hash != hash) {
                continue;
            }
            int cmp = PyObject_RichCompareBool(result->entry.key, key, Py_EQ);
            if (cmp < 0) {
                // exception thrown during compare
                goto error;
            } else if (cmp == 0) {
                continue;
            }
            goto found;
        }
    }  // probes exhausted

    not_found:
    if (is_compact != meta->is_compact) {
        goto beginning;
    }
    result->error = 0;
    result->found = 0;
    result->entry_p = NULL;
    return;
    error:
    result->error = 1;
    return;
    found:
    result->error = 0;
    result->found = 1;
    result->position = ix + probe + reservations;
    result->node = reader.node;
}

void
AtomicDict_LookupEntry(AtomicDict_Meta *meta, uint64_t entry_ix, Py_hash_t hash,
                       AtomicDict_SearchResult *result)
{
    uint64_t ix = AtomicDict_Distance0Of(hash, meta);
    uint8_t is_compact;
    uint64_t probe, reservations;
    AtomicDict_BufferedNodeReader reader;

    beginning:
    is_compact = meta->is_compact;
    reservations = 0;
    reader.zone = -1;

    for (probe = 0; probe < meta->size; probe++) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(ix + probe + reservations, &reader, meta);

        if (AtomicDict_NodeIsTombstone(&reader.node, meta)) {
            probe--;
            reservations++;
            continue;
        }

        if (AtomicDict_NodeIsReservation(&reader.node, meta)) {
            probe--;
            reservations++;
            result->is_reservation = 1;
            goto check_entry;
        }
        result->is_reservation = 0;

        if (reader.node.node == 0) {
            goto not_found;
        }

        if (
            is_compact && (
                (ix + probe + reservations - reader.node.distance > ix)
                || (probe >= meta->log_size)
            )) {
            goto not_found;
        }

        check_entry:
        if (reader.node.index == entry_ix) {
            goto found;
        }
    }  // probes exhausted

    not_found:
    if (is_compact != meta->is_compact) {
        goto beginning;
    }
    result->error = 0;
    result->found = 0;
    return;
    found:
    result->error = 0;
    result->found = 1;
    result->position = ix + probe + reservations;
    result->node = reader.node;
}


PyObject *
AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value)
{
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_SearchResult result;
    AtomicDict_Meta *meta = NULL;
    retry:
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);

    result.entry.value = NULL;
    AtomicDict_Lookup(meta, key, hash, &result);
    if (result.error)
        goto fail;

    if (AtomicRef_Get(self->metadata) != (PyObject *) meta) {
        Py_DECREF(meta);
        goto retry;
    }
    Py_DECREF(meta); // for atomic_ref_get_ref

    if (result.entry_p == NULL) {
        result.entry.value = default_value;
    }

    Py_DECREF(meta);
    return result.entry.value;
    fail:
    Py_XDECREF(meta);
    return NULL;
}

PyObject *
AtomicDict_GetItem(AtomicDict *self, PyObject *key)
{
    PyObject *value = AtomicDict_GetItemOrDefault(self, key, NULL);

    if (value == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
    } else {
        Py_INCREF(value);
    }

    return value;
}

PyObject *
AtomicDict_GetItemOrDefaultVarargs(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *key = NULL, *default_value = NULL;
    static char *keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", keywords, &key, &default_value))
        return NULL;

    if (default_value == NULL)
        default_value = Py_None;

    PyObject *value = AtomicDict_GetItemOrDefault(self, key, default_value);
    Py_INCREF(value);
    return value;
}
