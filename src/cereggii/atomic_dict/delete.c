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

        if (result.entry.value == NULL)
            goto not_found;

    } while (!CereggiiAtomic_CompareExchangePtr((void **) &result.entry_p->value,
                                                result.entry.value,
                                                NULL));

    do {
        if (CereggiiAtomic_CompareExchangeUInt8(
            &result.entry_p->flags,
            result.entry.flags,
            result.entry.flags | ENTRY_FLAGS_TOMBSTONE
        )) {
            result.entry.flags |= ENTRY_FLAGS_TOMBSTONE;
        }
        else {
            // what if swapped?
            AtomicDict_ReadEntry(result.entry_p, &result.entry);
        }
    } while (!(result.entry.flags & ENTRY_FLAGS_TOMBSTONE));

    uint64_t entry_ix = result.node.index;
    AtomicDict_BufferedNodeReader reader;
    AtomicDict_Node temp[16];
    int begin_write, end_write;
    int64_t _;

    do {
        AtomicDict_LookupEntry(meta, entry_ix, hash, &result);
        assert(!result.error);
        assert(result.position != 0);
        _ = 0;
        reader.zone = -1;
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(result.position, (AtomicDict_BufferedNodeReader *) &reader, meta);
        AtomicDict_CopyNodeBuffers(reader.buffer, temp);
        AtomicDict_RobinHoodDelete(meta, temp, reader.idx_in_buffer);
        AtomicDict_ComputeBeginEndWrite(meta, reader.buffer, temp, &begin_write, &end_write, &_);
    } while (!AtomicDict_AtomicWriteNodesAt(result.position - reader.idx_in_buffer + begin_write,
                                            end_write - begin_write,
                                            &reader.buffer[begin_write], &temp[begin_write], meta));

    uint64_t block_num;
    int64_t gab, gdb;
    AtomicDict_EntryLoc swap_loc;
    AtomicDict_Entry swap;

    recycle_entry:
    block_num = AtomicDict_BlockOf(entry_ix);
    gab = meta->greatest_allocated_block;
    gdb = meta->greatest_deleted_block;

    if (gdb > gab)
        goto recycle_entry;

    if (block_num < gab) {
        for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
            swap_loc.location = ((gdb + 1) << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) +
                                ((i + hash) % ATOMIC_DICT_ENTRIES_IN_BLOCK);
            swap_loc.entry = AtomicDict_GetEntryAt(swap_loc.location, meta);
            AtomicDict_ReadEntry(swap_loc.entry, &swap);

            if (!(swap.value == NULL || swap.flags & ENTRY_FLAGS_TOMBSTONE || swap.flags & ENTRY_FLAGS_SWAPPED))
                goto swap_found;
        }

        CereggiiAtomic_CompareExchangeInt64(&meta->greatest_deleted_block, gdb, gdb + 1);
        goto recycle_entry; // don't handle failure

        swap_found:
        result.entry_p->key = swap.key;
        result.entry_p->value = swap.value; // todo: what if value was updated? => use AtomicRef
        result.entry_p->hash = swap.hash;
        if (!CereggiiAtomic_CompareExchangeUInt8(
            &swap_loc.entry->flags,
            swap.flags,
            swap.flags | ENTRY_FLAGS_SWAPPED
        )) {
            AtomicDict_ReadEntry(swap_loc.entry, &swap);
            if (swap.value == NULL || swap.flags & ENTRY_FLAGS_TOMBSTONE || swap.flags & ENTRY_FLAGS_SWAPPED)
                goto recycle_entry;
        }

        CereggiiAtomic_StoreUInt8(&result.entry_p->flags, result.entry.flags & ~ENTRY_FLAGS_TOMBSTONE);

        AtomicDict_SearchResult swap_search;
        do_swap:
        AtomicDict_LookupEntry(meta, swap_loc.location, swap.hash, &swap_search);
        AtomicDict_Node swapped = {
            .tag = swap_search.node.tag,
            .distance = swap_search.node.distance,
            .index = entry_ix,
        };

        if (!AtomicDict_AtomicWriteNodesAt(swap_search.position, 1, &swap_search.node, &swapped, meta)) {
            goto do_swap;
        }
    }

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
