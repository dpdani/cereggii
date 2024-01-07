// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"
#include "atomic_ops.h"


int
AtomicDict_Delete(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash)
{
    atomic_dict_search_result result;
    AtomicDict_Lookup(meta, key, hash, &result);

    if (result.error)
        goto fail;

    if (result.entry_p == NULL)
        return 0;

    do {
        result.entry = *result.entry_p;  // READ

        if (result.entry.flags & ENTRY_FLAGS_TOMBSTONE)
            return 0;

    } while (!CereggiiAtomic_CompareExchangeUInt8(&result.entry_p->flags,
                                                  result.entry.flags,
                                                  result.entry.flags | ENTRY_FLAGS_TOMBSTONE));

    uint64_t entry_ix = result.node.index;

    // todo: do robin hood to check if we can clear the slot, instead of using a tombstone

    while (!AtomicDict_AtomicWriteNodesAt(result.position, 1, &result.node, &meta->tombstone, meta)) {
        AtomicDict_LookupEntry(meta, entry_ix, hash, &result);
        assert(!result.error);
        assert(result.entry_p != NULL);
    }

    // todo: recycle entry

    return 1;

    fail:
    return -1;
}

int
AtomicDict_DelItem(AtomicDict *self, PyObject *key)
{
    atomic_dict_meta *meta = (atomic_dict_meta *) AtomicRef_Get(self->metadata);
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    int deleted = AtomicDict_Delete(meta, key, hash);

    if (deleted < 0)
        goto fail;

    if (deleted == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }

    Py_DECREF(meta);
    return 0;

    fail:
    Py_XDECREF(meta);
    return -1;
}
