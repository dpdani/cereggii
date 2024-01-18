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

    do {
        AtomicDict_ReadEntry(result.entry_p, &result.entry);

        if (result.entry.flags & ENTRY_FLAGS_TOMBSTONE)
            goto not_found;

    } while (!CereggiiAtomic_CompareExchangeUInt8(&result.entry_p->flags,
                                                  result.entry.flags,
                                                  result.entry.flags | ENTRY_FLAGS_TOMBSTONE));

    uint64_t entry_ix = result.node.index;
    AtomicDict_BufferedNodeReader reader;
    AtomicDict_Node temp[16];
    int begin_write, end_write;
    int64_t start_ix = 0;

    do {
        AtomicDict_LookupEntry(meta, entry_ix, hash, &result);
        assert(!result.error);
        assert(result.entry_p != NULL);
        start_ix += (int64_t) result.position;
        start_ix = 0;
        reader.zone = -1;
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(result.position, (AtomicDict_BufferedNodeReader *) &reader, meta);
        AtomicDict_CopyNodeBuffers(reader.buffer, temp);
        AtomicDict_RobinHoodDelete(meta, temp, reader.idx_in_buffer);
        AtomicDict_ComputeBeginEndWrite(meta, reader.buffer, temp, &begin_write, &end_write, &start_ix);
        if (begin_write < 0)
            continue;
    } while (!AtomicDict_AtomicWriteNodesAt(result.position - reader.idx_in_buffer + begin_write, end_write - begin_write,
                                            &reader.buffer[begin_write], &temp[begin_write], meta));

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
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);

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
