// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"
#include <stdatomic.h>


void
delete_(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash, AtomicDictSearchResult *result)
{
    lookup(meta, key, hash, result);

    if (result->error) {
        return;
    }

    if (result->entry_p == NULL) {
        return;
    }

    while (!atomic_compare_exchange_strong_explicit(
        (_Atomic(PyObject *) *) &result->entry_p->value,
        &result->entry.value, NULL,
        memory_order_acq_rel, memory_order_acquire
    )) {
        read_entry(result->entry_p, &result->entry);

        if (result->entry.value == NULL) {
            result->found = 0;
            return;
        }
    }

    int ok = atomic_write_node_at(result->position, result->node, NODE_TOMBSTONE, meta);
    assert(ok);
    cereggii_unused_in_release_build(ok);
}

int
AtomicDict_DelItem(AtomicDict *self, PyObject *key)
{
    assert(key != NULL);

    AtomicDictMeta *meta = NULL;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = get_meta(self, storage);
    if (meta == NULL)
        goto fail;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    PyMutex_Lock(&storage->self_mutex);  // todo: maybe help migrate
    int migrated = maybe_help_migrate(meta, &storage->self_mutex);
    if (migrated) {
        // self_mutex was unlocked during the operation
        meta = NULL;
        goto beginning;
    }

    AtomicDictSearchResult result;
    delete_(meta, key, hash, &result);

    if (result.error) {
        PyMutex_Unlock(&storage->self_mutex);
        goto fail;
    }

    if (!result.found) {
        PyMutex_Unlock(&storage->self_mutex);
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }

    accessor_len_inc(self, storage, -1);
    accessor_tombstones_inc(self, storage, 1);

    PyMutex_Unlock(&storage->self_mutex);

    Py_DECREF(result.entry.key);
    Py_DECREF(result.entry.value);

    return 0;

    fail:
    return -1;
}
