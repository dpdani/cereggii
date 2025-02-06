// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"
#include "atomic_ops.h"


int
AtomicDict_Delete(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash)
{
    AtomicDict_SearchResult result;
    AtomicDict_Lookup(meta, key, hash, &result);

    if (result.error)
        goto fail;

    if (result.entry_p == NULL)
        goto not_found;

    while (!CereggiiAtomic_CompareExchangePtr((void **) &result.entry_p->value,
                                              result.entry.value,
                                              NULL)) {
        AtomicDict_ReadEntry(result.entry_p, &result.entry);

        if (result.entry.value == NULL)
            goto not_found;
    }

    Py_CLEAR(result.entry_p->key);
    Py_DECREF(result.entry.value);
    result.entry.value = NULL;

    AtomicDict_Node tombstone = {
        .index = 0,
        .tag = TOMBSTONE(meta),
    };

    int ok = AtomicDict_AtomicWriteNodeAt(result.position, &result.node, &tombstone, meta);
    assert(ok);

    return 1;

    not_found:
    return 0;

    fail:
    return -1;
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

    int deleted = AtomicDict_Delete(meta, key, hash);

    if (deleted > 0) {
        storage->local_len--;
        self->len_dirty = 1;
    }

    PyMutex_Unlock(&storage->self_mutex);

    if (deleted < 0)
        goto fail;

    if (deleted == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }

    return 0;

    fail:
    return -1;
}
