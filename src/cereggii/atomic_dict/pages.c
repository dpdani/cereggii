// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include <stdatomic.h>


AtomicDict_Page *
AtomicDictPage_New(void)
{
    AtomicDict_Page *new = NULL;
    new = PyObject_GC_New(AtomicDict_Page, &AtomicDictPage_Type);

    if (new == NULL)
        return NULL;

    cereggii_tsan_ignore_writes_begin();
    memset(new->entries, 0, sizeof(AtomicDict_PaddedEntry) * ATOMIC_DICT_ENTRIES_IN_PAGE);
    cereggii_tsan_ignore_writes_end();

    PyObject_GC_Track(new);

    return new;
}

int
AtomicDictPage_traverse(AtomicDict_Page *self, visitproc visit, void *arg)
{
    AtomicDictEntry entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_PAGE; ++i) {
        entry = self->entries[i].entry;

        if (entry.value == NULL)
            continue;

        Py_VISIT(entry.key);
        Py_VISIT(entry.value);
    }

    return 0;
}

int
AtomicDictPage_clear(AtomicDict_Page *self)
{
    AtomicDictEntry *entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_PAGE; ++i) {
        entry = &self->entries[i].entry;

        if (entry->value == NULL)
            continue;

        assert(entry->key != NULL);
        Py_CLEAR(entry->key);
        Py_CLEAR(entry->value);
    }

    return 0;
}

void
AtomicDictPage_dealloc(AtomicDict_Page *self)
{
    PyObject_GC_UnTrack(self);
    AtomicDictPage_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


int
atomic_dict_entry_ix_sanity_check(uint64_t entry_ix, AtomicDictMeta *meta)
{
    int64_t gap = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
    assert(gap >= 0);
    assert(page_of(entry_ix) <= (uint64_t) gap);
    cereggii_unused_in_release_build(entry_ix);
    cereggii_unused_in_release_build(gap);
    return 1;
}


int
get_empty_entry(AtomicDict *self, AtomicDictMeta *meta, AtomicDictReservationBuffer *rb,
                         AtomicDictEntryLoc *entry_loc, Py_hash_t hash)
{
    reservation_buffer_pop(rb, entry_loc, meta);

    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1) & ~(self->reservation_buffer_size - 1);
        int64_t inserting_page;

        reserve_in_inserting_page:
        inserting_page = atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_page, memory_order_acquire);
        for (int offset = 0; offset < ATOMIC_DICT_ENTRIES_IN_PAGE; offset += self->reservation_buffer_size) {
            AtomicDict_Page *page = atomic_load_explicit((_Atomic (AtomicDict_Page *) *) &meta->pages[inserting_page], memory_order_acquire);
            entry_loc->entry = &(page
                ->entries[(insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_PAGE]
                .entry);
            if (atomic_load_explicit((_Atomic (uint8_t) *) &entry_loc->entry->flags, memory_order_acquire) == 0) {
                uint8_t expected = 0;
                if (atomic_compare_exchange_strong_explicit((_Atomic(uint8_t) *) &entry_loc->entry->flags, &expected, ENTRY_FLAGS_RESERVED, memory_order_acq_rel, memory_order_acquire)) {
                    entry_loc->location =
                        (inserting_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) +
                        ((insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_PAGE);
                    assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
                    reservation_buffer_put(rb, entry_loc->location, self->reservation_buffer_size, meta);
                    reservation_buffer_pop(rb, entry_loc, meta);
                    goto done;
                }
            }
        }

        if (atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_page, memory_order_acquire) != inserting_page)
            goto reserve_in_inserting_page;

        int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
        if (greatest_allocated_page > inserting_page) {
            int64_t expected = inserting_page;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_page, &expected, inserting_page + 1, memory_order_acq_rel, memory_order_acquire);
            goto reserve_in_inserting_page; // even if the above CAS fails
        }
        assert(greatest_allocated_page >= 0);
        if ((uint64_t) greatest_allocated_page + 1u >= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
            return 0; // must grow
        }
        assert((uint64_t) greatest_allocated_page + 1u <= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE);

        AtomicDict_Page *page = NULL;
        page = AtomicDictPage_New();
        if (page == NULL)
            goto fail;

        page->entries[0].entry.flags = ENTRY_FLAGS_RESERVED;

        AtomicDict_Page *expected = NULL;
        int64_t new_page = greatest_allocated_page + 1;
        if (atomic_compare_exchange_strong_explicit((_Atomic(AtomicDict_Page *) *) &meta->pages[new_page], &expected, page, memory_order_acq_rel, memory_order_acquire)) {
            if ((uint64_t) greatest_allocated_page + 2u < (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
                atomic_store_explicit((_Atomic(AtomicDict_Page *) *) &meta->pages[new_page + 1], NULL, memory_order_release);
            }
            int64_t expected2 = greatest_allocated_page;
            int ok = atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->greatest_allocated_page,
                                                    &expected2, new_page, memory_order_acq_rel, memory_order_acquire);
            assert(ok);
            cereggii_unused_in_release_build(ok);
            expected2 = greatest_allocated_page;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_page,
                                                    &expected2, new_page, memory_order_acq_rel, memory_order_acquire);
            // this cas may fail because another thread helped increasing this counter
            entry_loc->entry = &(page->entries[0].entry);
            entry_loc->location = new_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE;
            assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
            reservation_buffer_put(rb, entry_loc->location, self->reservation_buffer_size, meta);
            reservation_buffer_pop(rb, entry_loc, meta);
        } else {
            Py_DECREF(page);
            goto reserve_in_inserting_page;
        }
    }

    done:
    assert(entry_loc->entry != NULL);
    assert(entry_loc->entry->key == NULL);
    assert(entry_loc->entry->value == NULL);
    assert(entry_loc->entry->hash == 0);
    assert(entry_loc->location > 0);
    assert(entry_loc->location < (uint64_t) SIZE_OF(meta));
    assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
    return 1;
    fail:
    entry_loc->entry = NULL;
    return -1;
}

uint64_t
page_of(uint64_t entry_ix)
{
    return entry_ix >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE;
}

uint64_t
position_in_page_of(uint64_t entry_ix)
{
    return entry_ix & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1);
}

AtomicDictEntry *
get_entry_at(uint64_t ix, AtomicDictMeta *meta)
{
    assert(atomic_dict_entry_ix_sanity_check(ix, meta));
    AtomicDict_Page *page = atomic_load_explicit((_Atomic (AtomicDict_Page *) *) &meta->pages[page_of(ix)], memory_order_acquire);
    assert(page != NULL);
    return &(
        page
            ->entries[position_in_page_of(ix)]
            .entry
    );
}

void
read_entry(AtomicDictEntry *entry_p, AtomicDictEntry *entry)
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
