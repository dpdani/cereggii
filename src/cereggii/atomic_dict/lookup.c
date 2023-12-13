// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"


void
AtomicDict_Lookup(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                  int look_into_reservations, atomic_dict_search_result *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    uint64_t ix = hash & ((1 << meta->log_size) - 1);
    int probe;
    int reservations = 0;

    for (probe = 0; probe < meta->log_size; probe++) {
        AtomicDict_ReadNodeAt((ix + probe + reservations) % (1 << meta->log_size), &result->node, meta);

        if (AtomicDict_NodeIsReservation(&result->node, meta)) {
            probe--;
            reservations++;
            if (look_into_reservations) {
                result->is_reservation = 1;
                goto check_entry;
            } else {
                continue;
            }
        }
        result->is_reservation = 0;

        if (result->node.node == 0) {
            goto not_found;
        }

        if (ix + probe + reservations - result->node.distance > ix) {
            goto not_found;
        }

        if (result->node.tag == (hash & meta->tag_mask)) {
            check_entry:
            result->entry_p = &(meta
                ->blocks[result->node.index >> 6]
                ->entries[result->node.index & 63]);
            result->entry = *result->entry_p; // READ

            if (result->entry.flags & ENTRY_FLAGS_TOMBSTONE) {
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
    result->error = 0;
    result->entry_p = NULL;
    return;
    error:
    result->error = 1;
    return;
    found:
    result->error = 0;
    result->index = ix;
}

PyObject *
AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value)
{
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    atomic_dict_search_result result;
    atomic_dict_meta *meta;
    retry:
    meta = (atomic_dict_meta *) AtomicRef_Get(self->metadata);

    result.entry.value = NULL;
    AtomicDict_Lookup(meta, key, hash, 0, &result);
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
