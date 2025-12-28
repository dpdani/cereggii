// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"
#include <stdatomic.h>


void
AtomicDict_Delete(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash, AtomicDict_SearchResult *result)
{
    AtomicDict_Lookup(meta, key, hash, result);

    if (result->error) {
        return;
    }

    if (result->entry_p == NULL) {
        return;
    }

    while (!atomic_compare_exchange_strong_explicit((_Atomic(void *) *) &result->entry_p->value,
                                              &result->entry.value,
                                              NULL, memory_order_acq_rel,
                                                   memory_order_acquire)) {
        AtomicDict_ReadEntry(result->entry_p, &result->entry);

        if (result->entry.value == NULL) {
            result->found = 0;
            return;
        }
    }

    AtomicDict_Node tombstone = {
        .index = 0,
        .tag = TOMBSTONE(meta),
    };

    int ok = AtomicDict_AtomicWriteNodeAt(result->position, &result->node, &tombstone, meta);
    assert(ok);
}

int
AtomicDict_DelItem(AtomicDict *self, PyObject *key)
{
    assert(key != NULL);

    AtomicDict_Meta *meta = NULL;
    AtomicDict_AccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = AtomicDict_GetMeta(self, storage);
    if (meta == NULL)
        goto fail;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    PyMutex_Lock(&storage->self_mutex);
    int migrated = AtomicDict_MaybeHelpMigrate(meta, &storage->self_mutex, self->accessors);
    if (migrated) {
        // self_mutex was unlocked during the operation
        meta = NULL;
        goto beginning;
    }

    AtomicDict_SearchResult result;
    AtomicDict_Delete(meta, key, hash, &result);

    PyMutex_Unlock(&storage->self_mutex);

    if (result.error) {
        goto fail;
    }

    if (!result.found) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }

    storage->local_len--;
    self->len_dirty = 1;
    Py_DECREF(result.entry.key);
    Py_DECREF(result.entry.value);

    return 0;

    fail:
    return -1;
}
