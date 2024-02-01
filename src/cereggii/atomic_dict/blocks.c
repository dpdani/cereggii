// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"


AtomicDict_Block *
AtomicDict_NewBlock(AtomicDict_Meta *meta)
{
    AtomicDict_Block *new = NULL;
    new = PyMem_RawMalloc(sizeof(AtomicDict_Block));

    if (new == NULL)
        return NULL;

    Py_INCREF(meta->generation);
    new->generation = meta->generation;
    memset(new->entries, 0, sizeof(AtomicDict_Entry) * ATOMIC_DICT_ENTRIES_IN_BLOCK);

    return new;
}

void
AtomicDict_FreeBlock(AtomicDict_Block *block_ptr)
{
    Py_DECREF(block_ptr->generation);

    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
        AtomicDict_Entry entry = block_ptr->entries[i];

        if (entry.flags & ENTRY_FLAGS_TOMBSTONE || entry.flags & ENTRY_FLAGS_SWAPPED)
            continue;

        Py_XDECREF(entry.key);
        Py_XDECREF(entry.value);
    }

    PyMem_RawFree(block_ptr);
}

void
AtomicDict_FreeBlockInMeta(AtomicDict_Meta *meta, uint64_t block_i)
{
    AtomicDict_Block *block_ptr = meta->blocks[block_i];
    int success = CereggiiAtomic_CompareExchangePtr((void **) &meta->blocks[block_i], block_ptr, NULL);
    assert(success);

    AtomicDict_FreeBlock(block_ptr);
}


int
AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
                         AtomicDict_EntryLoc *entry_loc, Py_hash_t hash)
{
    AtomicDict_ReservationBufferPop(rb, entry_loc);

    beginning:
    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1) & ~(self->reservation_buffer_size - 1);
        int64_t inserting_block;

        reserve_in_inserting_block:
        inserting_block = meta->inserting_block;
        for (int offset = 0; offset < ATOMIC_DICT_ENTRIES_IN_BLOCK; offset += self->reservation_buffer_size) {
            entry_loc->entry = &meta
                ->blocks[inserting_block]
                ->entries[(insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_BLOCK];
            if (entry_loc->entry->flags == 0) {
                if (CereggiiAtomic_CompareExchangeUInt8(&entry_loc->entry->flags, 0, ENTRY_FLAGS_RESERVED)) {
                    entry_loc->location = insert_position + offset;
                    AtomicDict_ReservationBufferPut(rb, entry_loc, self->reservation_buffer_size);
                    AtomicDict_ReservationBufferPop(rb, entry_loc);
                    goto done;
                }
            }
        }

        if (meta->inserting_block != inserting_block)
            goto reserve_in_inserting_block;

        int64_t greatest_allocated_block = meta->greatest_allocated_block;
        if (greatest_allocated_block > inserting_block) {
            CereggiiAtomic_CompareExchangeInt64(&meta->inserting_block, inserting_block, inserting_block + 1);
            goto reserve_in_inserting_block; // even if the above CAS fails
        }
        if (greatest_allocated_block + 1 >= meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
            return 0; // must grow
        }
        assert(greatest_allocated_block + 1 <= meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);

        AtomicDict_Block *block = NULL;
        block = AtomicDict_NewBlock(meta);
        if (block == NULL)
            goto fail;

        block->entries[0].flags = ENTRY_FLAGS_RESERVED;

        if (CereggiiAtomic_CompareExchangePtr((void **) &meta->blocks[greatest_allocated_block + 1], NULL, block)) {
            if (greatest_allocated_block + 2 < meta->size >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
                meta->blocks[greatest_allocated_block + 2] = NULL;
            }
            int success;
            success = CereggiiAtomic_CompareExchangeInt64(&meta->greatest_allocated_block,
                                                          greatest_allocated_block,
                                                          greatest_allocated_block + 1);
            assert(success);
            success = CereggiiAtomic_CompareExchangeInt64(&meta->inserting_block,
                                                          greatest_allocated_block,
                                                          greatest_allocated_block + 1);
            assert(success);
            entry_loc->entry = &block->entries[0];
            entry_loc->location = (greatest_allocated_block + 1) << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK;
            AtomicDict_ReservationBufferPut(rb, entry_loc, self->reservation_buffer_size);
            AtomicDict_ReservationBufferPop(rb, entry_loc);
        } else {
            AtomicDict_FreeBlock(block);
            goto reserve_in_inserting_block;
        }
    }

    done:
    assert(entry_loc->entry != NULL);
    assert(entry_loc->entry->key == NULL);
    return 1;
    fail:
    entry_loc->entry = NULL;
    return -1;
}

inline uint64_t
AtomicDict_BlockOf(uint64_t entry_ix)
{
    return entry_ix >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK;
}

inline uint64_t
AtomicDict_PositionInBlockOf(uint64_t entry_ix)
{
    return entry_ix & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1);
}

inline AtomicDict_Entry *
AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta)
{
    assert(AtomicDict_BlockOf(ix) <= meta->greatest_allocated_block);
    return &(
        meta->blocks[AtomicDict_BlockOf(ix)]
            ->entries[AtomicDict_PositionInBlockOf(ix)]
    );
}

inline void
AtomicDict_ReadEntry(AtomicDict_Entry *entry_p, AtomicDict_Entry *entry)
{
    entry->flags = entry_p->flags;
    entry->value = entry_p->value;
    if (entry->value == NULL || entry->flags & ENTRY_FLAGS_TOMBSTONE || entry->flags & ENTRY_FLAGS_SWAPPED) {
        entry->key = NULL;
        entry->value = NULL;
        entry->hash = -1;
        return;
    }
    entry->key = entry_p->key;
    entry->value = entry_p->value;
    entry->hash = entry_p->hash;
}
