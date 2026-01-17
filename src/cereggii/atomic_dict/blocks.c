// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include <stdatomic.h>


AtomicDict_Block *
AtomicDictBlock_New(AtomicDict_Meta *meta)
{
    AtomicDict_Block *new = NULL;
    new = PyObject_GC_New(AtomicDict_Block, &AtomicDictBlock_Type);

    if (new == NULL)
        return NULL;

    new->generation = meta->generation;
    cereggii_tsan_ignore_writes_begin();
    memset(new->entries, 0, sizeof(AtomicDict_PaddedEntry) * ATOMIC_DICT_ENTRIES_IN_BLOCK);
    cereggii_tsan_ignore_writes_end();

    PyObject_GC_Track(new);

    return new;
}

int
AtomicDictBlock_traverse(AtomicDict_Block *self, visitproc visit, void *arg)
{
    AtomicDict_Entry entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
        entry = self->entries[i].entry;

        if (entry.value == NULL)
            continue;

        Py_VISIT(entry.key);
        Py_VISIT(entry.value);
    }

    return 0;
}

int
AtomicDictBlock_clear(AtomicDict_Block *self)
{
    AtomicDict_Entry *entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_BLOCK; ++i) {
        entry = &self->entries[i].entry;

        if (entry->value == NULL)
            continue;

        Py_CLEAR(entry->key);
        Py_CLEAR(entry->value);
    }

    return 0;
}

void
AtomicDictBlock_dealloc(AtomicDict_Block *self)
{
    PyObject_GC_UnTrack(self);
    AtomicDictBlock_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


int
AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
                         AtomicDict_EntryLoc *entry_loc, Py_hash_t hash)
{
    AtomicDict_ReservationBufferPop(rb, entry_loc);

    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1) & ~(self->reservation_buffer_size - 1);
        int64_t inserting_block;

        reserve_in_inserting_block:
        inserting_block = atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_block, memory_order_acquire);
        for (int offset = 0; offset < ATOMIC_DICT_ENTRIES_IN_BLOCK; offset += self->reservation_buffer_size) {
            AtomicDict_Block *block = atomic_load_explicit((_Atomic (AtomicDict_Block *) *) &meta->blocks[inserting_block], memory_order_acquire);
            entry_loc->entry = &(block
                ->entries[(insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_BLOCK]
                .entry);
            if (atomic_load_explicit((_Atomic (uint8_t) *) &entry_loc->entry->flags, memory_order_acquire) == 0) {
                uint8_t expected = 0;
                if (atomic_compare_exchange_strong_explicit((_Atomic(uint8_t) *) &entry_loc->entry->flags, &expected, ENTRY_FLAGS_RESERVED, memory_order_acq_rel, memory_order_acquire)) {
                    entry_loc->location =
                        (inserting_block << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) +
                        ((insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_BLOCK);
                    int64_t gab = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_block, memory_order_acquire);
                    assert(gab >= 0);
                    assert(AtomicDict_BlockOf(entry_loc->location) <= (uint64_t) gab);
                    cereggii_unused_in_release_build(gab);
                    AtomicDict_ReservationBufferPut(rb, entry_loc, self->reservation_buffer_size, meta);
                    AtomicDict_ReservationBufferPop(rb, entry_loc);
                    goto done;
                }
            }
        }

        if (atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_block, memory_order_acquire) != inserting_block)
            goto reserve_in_inserting_block;

        int64_t greatest_allocated_block = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_block, memory_order_acquire);
        if (greatest_allocated_block > inserting_block) {
            int64_t expected = inserting_block;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_block, &expected, inserting_block + 1, memory_order_acq_rel, memory_order_acquire);
            goto reserve_in_inserting_block; // even if the above CAS fails
        }
        assert(greatest_allocated_block >= 0);
        if ((uint64_t) greatest_allocated_block + 1u >= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
            return 0; // must grow
        }
        assert((uint64_t) greatest_allocated_block + 1u <= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);

        AtomicDict_Block *block = NULL;
        block = AtomicDictBlock_New(meta);
        if (block == NULL)
            goto fail;

        block->entries[0].entry.flags = ENTRY_FLAGS_RESERVED;

        void *expected = NULL;
        if (atomic_compare_exchange_strong_explicit((_Atomic(void *) *) &meta->blocks[greatest_allocated_block + 1], &expected, block, memory_order_acq_rel, memory_order_acquire)) {
            if ((uint64_t) greatest_allocated_block + 2u < (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK) {
                atomic_store_explicit((_Atomic(void *) *) &meta->blocks[greatest_allocated_block + 2], NULL, memory_order_release);
            }
            int64_t expected2 = greatest_allocated_block;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->greatest_allocated_block,
                                                    &expected2,
                                                    greatest_allocated_block + 1, memory_order_acq_rel, memory_order_acquire);
            expected2 = greatest_allocated_block;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_block,
                                                    &expected2,
                                                    greatest_allocated_block + 1, memory_order_acq_rel, memory_order_acquire);
            entry_loc->entry = &(block->entries[0].entry);
            entry_loc->location = (greatest_allocated_block + 1) << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK;
            int64_t gab = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_block, memory_order_acquire);
            assert(gab >= 0);
            assert(AtomicDict_BlockOf(entry_loc->location) <= (uint64_t) gab);
            cereggii_unused_in_release_build(gab);
            AtomicDict_ReservationBufferPut(rb, entry_loc, self->reservation_buffer_size, meta);
            AtomicDict_ReservationBufferPop(rb, entry_loc);
        } else {
            Py_DECREF(block);
            goto reserve_in_inserting_block;
        }
    }

    done:
    assert(entry_loc->entry != NULL);
    assert(entry_loc->entry->key == NULL);
    assert(entry_loc->location < (uint64_t) SIZE_OF(meta));
    return 1;
    fail:
    entry_loc->entry = NULL;
    return -1;
}

uint64_t
AtomicDict_BlockOf(uint64_t entry_ix)
{
    return entry_ix >> ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK;
}

uint64_t
AtomicDict_PositionInBlockOf(uint64_t entry_ix)
{
    return entry_ix & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1);
}

AtomicDict_Entry *
AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta)
{
    int64_t gab = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_block, memory_order_acquire);
    assert(gab >= 0);
    assert(AtomicDict_BlockOf(ix) <= (uint64_t) gab);
    cereggii_unused_in_release_build(gab);
    AtomicDict_Block *block = atomic_load_explicit((_Atomic (AtomicDict_Block *) *) &meta->blocks[AtomicDict_BlockOf(ix)], memory_order_acquire);
    assert(block != NULL);
    return &(
        block
            ->entries[AtomicDict_PositionInBlockOf(ix)]
            .entry
    );
}

void
AtomicDict_ReadEntry(AtomicDict_Entry *entry_p, AtomicDict_Entry *entry)
{
    entry->flags = atomic_load_explicit((_Atomic(uint8_t) *) &entry_p->flags, memory_order_acquire);
    entry->value = atomic_load_explicit((_Atomic(PyObject *) *) &entry_p->value, memory_order_acquire);
    if (entry->value == NULL) {
        entry->key = NULL;
        entry->value = NULL;
        entry->hash = -1;
        return;
    }
    entry->key = atomic_load_explicit((_Atomic(PyObject *) *) &entry_p->key, memory_order_acquire);
    entry->hash = atomic_load_explicit((_Atomic(Py_hash_t) *) &entry_p->hash, memory_order_acquire);
}
