// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "constants.h"
#include "_internal_py_core.h"


void
lookup(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                  AtomicDictSearchResult *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    const uint64_t d0 = distance0_of(hash, meta);
    uint64_t distance = 0;

    for (; distance < (1ull << meta->log_size); distance++) {
        read_node_at(d0 + distance, &result->node, meta);

        if (is_empty(&result->node))
            goto not_found;
        if (is_tombstone(&result->node))
            continue;

        if (result->node.tag == (hash & TAG_MASK(meta))) {
            result->entry_p = get_entry_at(result->node.index, meta);
            read_entry(result->entry_p, &result->entry);

            if (result->entry.value == NULL)
                continue;
            if (result->entry.key == key)
                goto found;
            if (result->entry.hash != hash)
                continue;

            int cmp = PyObject_RichCompareBool(result->entry.key, key, Py_EQ);
            if (cmp < 0) {
                // exception thrown during compare
                goto error;
            }
            if (cmp == 0) {
                continue;
            }
            goto found;
        }
    }

    // have looped over the entire index without finding the key => not found

    not_found:
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
    result->position = (d0 + distance) & ((1 << meta->log_size) - 1);
}

void
lookup_entry(AtomicDictMeta *meta, uint64_t entry_ix, Py_hash_t hash,
                       AtomicDictSearchResult *result)
{
    // index-only search

    const uint64_t d0 = distance0_of(hash, meta);
    uint64_t distance = 0;

    for (; distance < 1ull << meta->log_size; distance++) {
        read_node_at(d0 + distance, &result->node, meta);

        if (is_empty(&result->node))
            goto not_found;
        if (result->node.index == entry_ix)
            goto found;
    }  // probes exhausted

    not_found:
    result->error = 0;
    result->found = 0;
    return;
    found:
    result->error = 0;
    result->found = 1;
    result->position = (d0 + distance) & ((1 << meta->log_size) - 1);
}


PyObject *
AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value)
{
    AtomicDictMeta *meta = NULL;
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDictSearchResult result;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    retry:
    meta = get_meta(self, storage);

    result.entry.value = NULL;
    lookup(meta, key, hash, &result);
    if (result.error)
        goto fail;
    if (result.entry_p == NULL) {
        result.entry.value = default_value;
    }
    if (result.entry.value == NULL)
        return NULL;
    if (!_Py_TryIncref(result.entry.value))
        goto retry;

    return result.entry.value;
    fail:
    return NULL;
}

PyObject *
AtomicDict_GetItem(AtomicDict *self, PyObject *key)
{
    PyObject *value = NULL;
    value = AtomicDict_GetItemOrDefault(self, key, NULL);

    if (value == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_SetObject(PyExc_KeyError, key);
        }
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

    return AtomicDict_GetItemOrDefault(self, key, default_value);
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
    AtomicDictMeta *meta = NULL;

    hashes = PyMem_RawMalloc(chunk_size * sizeof(Py_hash_t));
    if (hashes == NULL)
        goto fail;
    keys = PyMem_RawMalloc(chunk_size * sizeof(PyObject *));
    if (keys == NULL)
        goto fail;

    AtomicDictSearchResult result;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    retry:
    meta = get_meta(self, storage);
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

        cereggii_prefetch(&meta->index[distance0_of(hash, meta)]);

        if (chunk_end % chunk_size == 0)
            break;
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        uint64_t d0 = distance0_of(hash, meta);
        AtomicDictNode node;

        read_node_at(d0, &node, meta);

        if (is_empty(&node))
            continue;
        if (is_tombstone(&node))
            continue;

        if (node.tag == (hash & TAG_MASK(meta))) {
            cereggii_prefetch(get_entry_at(node.index, meta));
        }
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        result.found = 0;
        lookup(meta, key, hash, &result);
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

    if (get_meta(self, storage) != meta)
        goto retry;

    PyMem_RawFree(hashes);
    PyMem_RawFree(keys);
    Py_INCREF(batch);
    return batch;
    fail:
    if (hashes != NULL) {
        PyMem_RawFree(hashes);
    }
    if (keys != NULL) {
        PyMem_RawFree(keys);
    }
    return NULL;
}
