// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


atomic_dict_block *
AtomicDict_NewBlock(atomic_dict_meta *meta)
{
    atomic_dict_block *new = PyMem_RawMalloc(sizeof(atomic_dict_block));

    if (new == NULL)
        return NULL;

    new->generation = meta->generation;
    memset(new->entries, 0, sizeof(atomic_dict_entry) * 64);

    return new;
}


void
AtomicDict_GetEmptyEntry(AtomicDict *dk, atomic_dict_meta *meta, atomic_dict_reservation_buffer *rb,
                         atomic_dict_entry_loc *entry_loc, Py_hash_t hash)
{
    AtomicDict_ReservationBufferPop(rb, entry_loc);

    beginning:
    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & 63 & ~(dk->reservation_buffer_size - 1);
        int64_t inserting_block;

        reserve_in_inserting_block:
        inserting_block = meta->inserting_block;
        for (int offset = 0; offset < 64; offset += dk->reservation_buffer_size) {
            entry_loc->entry = &meta
                ->blocks[inserting_block]
                ->entries[(insert_position + offset) % 64];
            if (entry_loc->entry->flags == 0) {
                if (__sync_bool_compare_and_swap_1(&entry_loc->entry->flags, 0, ENTRY_FLAGS_RESERVED)) {
                    entry_loc->location = insert_position + offset;
                    AtomicDict_ReservationBufferPut(rb, entry_loc, dk->reservation_buffer_size);
                    AtomicDict_ReservationBufferPop(rb, entry_loc);
                    goto done;
                }
            }
        }

        if (meta->inserting_block != inserting_block)
            goto reserve_in_inserting_block;

        int64_t greatest_allocated_block = meta->greatest_allocated_block;
        if (greatest_allocated_block > inserting_block) {
            _Py_atomic_compare_exchange_int64(&meta->inserting_block, inserting_block, inserting_block + 1);
            goto reserve_in_inserting_block; // even if the above CAS fails
        }
        if (greatest_allocated_block + 1 >= (1 << meta->log_size) >> 6) {
            AtomicDict_Grow(dk);
            goto beginning;
        }
        assert(greatest_allocated_block + 1 <= (1 << meta->log_size) >> 6);

        atomic_dict_block *block = AtomicDict_NewBlock(meta);
        if (block == NULL)
            goto fail;
        block->entries[0].flags = ENTRY_FLAGS_RESERVED;

        if (_Py_atomic_compare_exchange_ptr(&meta->blocks[greatest_allocated_block + 1], NULL, block)) {
            if (greatest_allocated_block + 2 < (1 << meta->log_size) >> 6) {
                meta->blocks[greatest_allocated_block + 2] = NULL;
            }
            assert(_Py_atomic_compare_exchange_int64(&meta->greatest_allocated_block,
                                                     greatest_allocated_block,
                                                     greatest_allocated_block + 1));
            assert(_Py_atomic_compare_exchange_int64(&meta->inserting_block,
                                                     greatest_allocated_block,
                                                     greatest_allocated_block + 1));
            entry_loc->entry = &block->entries[0];
            entry_loc->location = (greatest_allocated_block + 1) << 6;
            AtomicDict_ReservationBufferPut(rb, entry_loc, dk->reservation_buffer_size);
        } else {
            PyMem_RawFree(block);
        }
        goto reserve_in_inserting_block;
    }

    done:
    assert(entry_loc->entry != NULL);
    assert(entry_loc->entry->key == NULL);
    return;
    fail:
    entry_loc->entry = NULL;
}

inline atomic_dict_entry *
AtomicDict_GetEntryAt(uint64_t ix, atomic_dict_meta *meta)
{
    return &(meta->blocks[ix >> 6]->entries[ix & 63]);
}
