// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "constants.h"


void
AtomicDict_Lookup(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                  AtomicDict_SearchResult *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    uint64_t ix = AtomicDict_Distance0Of(hash, meta);
    uint8_t is_compact;
    uint64_t probe, reservations;
    AtomicDict_Node node;

    beginning:
    is_compact = meta->is_compact;
    reservations = 0;

    for (probe = 0; probe < meta->size; probe++) {
        AtomicDict_ReadNodeAt(ix + probe + reservations, &node, meta);

        if (AtomicDict_NodeIsReservation(&node, meta)) {
            probe--;
            reservations++;
            result->is_reservation = 1;
            goto check_entry;
        }
        result->is_reservation = 0;

        if (node.node == 0) {
            goto not_found;
        }

        if (
            is_compact && (
                (ix + probe + reservations - node.distance > ix)
                || (probe >= meta->max_distance)
            )) {
            goto not_found;
        }

        check_entry:
        if (node.tag == (hash & meta->tag_mask)) {
            result->entry_p = AtomicDict_GetEntryAt(node.index, meta);
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
    result->position = (ix + probe + reservations) & (meta->size - 1);
    result->node = node;
}

void
AtomicDict_LookupEntry(AtomicDict_Meta *meta, uint64_t entry_ix, Py_hash_t hash,
                       AtomicDict_SearchResult *result)
{
    // index-only search

    uint64_t ix = AtomicDict_Distance0Of(hash, meta);
    uint8_t is_compact;
    uint64_t probe, reservations;
    AtomicDict_Node node;

    beginning:
    is_compact = meta->is_compact;
    reservations = 0;

    for (probe = 0; probe < meta->size; probe++) {
        AtomicDict_ReadNodeAt(ix + probe + reservations, &node, meta);

        if (AtomicDict_NodeIsTombstone(&node, meta)) {
            probe--;
            reservations++;
            continue;
        }

        if (AtomicDict_NodeIsReservation(&node, meta)) {
            probe--;
            reservations++;
            result->is_reservation = 1;
            goto check_entry;
        }
        result->is_reservation = 0;

        if (node.node == 0) {
            goto not_found;
        }

        if (
            is_compact && (
                (ix + probe + reservations - node.distance > ix)
                || (probe >= meta->max_distance)
            )) {
            goto not_found;
        }

        check_entry:
        if (node.index == entry_ix) {
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
    result->position = (ix + probe + reservations) & (meta->size - 1);
    result->node = node;
}


PyObject *
AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value)
{
    AtomicDict_Meta *meta = NULL;
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_SearchResult result;
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
    Py_DECREF(meta); // for AtomicRef_Get (if condition)

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

PyObject *
AtomicDict_BatchGetItem(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *batch = NULL;
    Py_ssize_t chunk_size = 128;

    char *kw_list[] = {"batch", "chunk_size", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|n", kw_list, &batch, &chunk_size))
        return NULL;

    if (!PyDict_CheckExact(batch)) {
        PyErr_SetString(PyExc_TypeError, "type(batch) != dict");
        return NULL;
    }

    if (chunk_size <= 0) {
        PyErr_SetString(PyExc_ValueError, "chunk_size <= 0");
        return NULL;
    }

    PyObject *key, *value;
    Py_hash_t hash;
    Py_hash_t *hashes = NULL;
    PyObject **keys = NULL;
    AtomicDict_Meta *meta = NULL;

    hashes = PyMem_RawMalloc(chunk_size * sizeof(Py_hash_t));
    if (hashes == NULL)
        goto fail;

    keys = PyMem_RawMalloc(chunk_size * sizeof(PyObject *));
    if (keys == NULL)
        goto fail;

    AtomicDict_SearchResult result;
    retry:
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);

    if (meta == NULL)
        goto fail;

    Py_ssize_t chunk_start = 0, chunk_end = 0;
    Py_ssize_t pos = 0;

    Py_BEGIN_CRITICAL_SECTION(batch);

    next_chunk:
    while (PyDict_Next(batch, &pos, &key, &value)) {
        chunk_end++;
        hash = PyObject_Hash(key);
        if (hash == -1)
            goto fail;

        hashes[(chunk_end - 1) % chunk_size] = hash;
        keys[(chunk_end - 1) % chunk_size] = key;

        __builtin_prefetch(AtomicDict_IndexAddressOf(AtomicDict_Distance0Of(hash, meta), meta));

        if (chunk_end % chunk_size == 0)
            break;
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        uint64_t d0 = AtomicDict_Distance0Of(hash, meta);
        AtomicDict_Node node;

        for (uint64_t j = d0; j < d0 + meta->max_distance; ++j) {
            AtomicDict_ReadNodeAt(j, &node, meta);

            if (node.node == 0)
                break;

            if (AtomicDict_NodeIsTombstone(&node, meta))
                continue;

            if (node.tag == (hash & meta->tag_mask)) {
                __builtin_prefetch(AtomicDict_GetEntryAt(node.index, meta));
                break;
            }
        }
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        result.found = 0;
        AtomicDict_Lookup(meta, key, hash, &result);
        if (result.error)
            goto fail;

        assert(_PyDict_GetItem_KnownHash(batch, key, hash) != NULL); // returns a borrowed reference
        if (result.found) {
            if (PyDict_SetItem(batch, key, result.entry.value) < 0)
                goto fail;
        } else {
            if (PyDict_SetItem(batch, key, NOT_FOUND) < 0)
                goto fail;
        }
    }

    if (PyDict_Size(batch) != chunk_end) {
        chunk_start = chunk_end;
        goto next_chunk;
    }

    Py_END_CRITICAL_SECTION();

    if (self->metadata->reference != (PyObject *) meta) {
        Py_DECREF(meta);
        goto retry;
    }

    Py_DECREF(meta);
    PyMem_RawFree(hashes);
    PyMem_RawFree(keys);
    Py_INCREF(batch);
    return batch;
    fail:
    Py_XDECREF(meta);
    if (hashes != NULL) {
        PyMem_RawFree(hashes);
    }
    if (keys != NULL) {
        PyMem_RawFree(keys);
    }
    return NULL;
}
