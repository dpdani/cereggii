// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdatomic.h>

#include "atomic_dict.h"
#include "atomic_dict_internal.h"


AtomicDictAccessorStorage *
AtomicDict_GetOrCreateAccessorStorage(AtomicDict *self)
{
    assert(self->accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(self->accessor_key);

    if (storage == NULL) {
        storage = PyMem_RawMalloc(sizeof(AtomicDictAccessorStorage));
        if (storage == NULL)
            return NULL;

        storage->next_accessor = NULL;
        storage->self_mutex = (PyMutex) {0};
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_len, 0, memory_order_release);
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_inserted, 0, memory_order_release);
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_tombstones, 0, memory_order_release);
        storage->reservation_buffer.head = 0;
        storage->reservation_buffer.tail = 0;
        storage->reservation_buffer.count = 0;
        memset(storage->reservation_buffer.reservations, 0, sizeof(AtomicDictEntryLoc) * RESERVATION_BUFFER_SIZE);
        storage->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);

        int set = PyThread_tss_set(self->accessor_key, storage);
        if (set != 0)
            goto fail;

        PyMutex_Lock(&self->accessors_lock);
        storage->accessor_ix = self->accessors_len;
        if (self->accessors == NULL) {
            self->accessors = storage;
            self->accessors_len = 1;
        } else {
            AtomicDictAccessorStorage *s = NULL;
            for (s = self->accessors; s->next_accessor != NULL; s = s->next_accessor) {}
            assert(s != NULL);
            atomic_store_explicit((_Atomic (AtomicDictAccessorStorage *) *) &s->next_accessor, storage, memory_order_release);
            self->accessors_len++;
        }
        PyMutex_Unlock(&self->accessors_lock);
    }

    return storage;
    fail:
    assert(storage != NULL);
    Py_XDECREF(storage->meta);
    PyMem_RawFree(storage);
    return NULL;
}

AtomicDictAccessorStorage *
AtomicDict_GetAccessorStorage(Py_tss_t *accessor_key)
{
    assert(accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(accessor_key);
    assert(storage != NULL);
    return storage;
}

void
AtomicDict_FreeAccessorStorage(AtomicDictAccessorStorage *self)
{
    Py_CLEAR(self->meta);
    PyMem_RawFree(self);
}

void
AtomicDict_FreeAccessorStorageList(AtomicDictAccessorStorage *head)
{
    if (head == NULL)
        return;

    AtomicDictAccessorStorage *prev = head;
    AtomicDictAccessorStorage *next = head->next_accessor;

    while (next) {
        AtomicDict_FreeAccessorStorage(prev);
        prev = next;
        next = next->next_accessor;
    }

    AtomicDict_FreeAccessorStorage(prev);
}

AtomicDictMeta *
AtomicDict_GetMeta(AtomicDict *self, AtomicDictAccessorStorage *storage)
{
    assert(storage != NULL);
    PyObject *shared = self->metadata->reference;
    PyObject *mine = (PyObject *) storage->meta;
    if (shared == mine)
        return storage->meta;

    Py_CLEAR(storage->meta);
    storage->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);
    assert(storage->meta != NULL);
    return storage->meta;
}

void
AtomicDict_BeginSynchronousOperation(AtomicDict *self)
{
    PyMutex_Lock(&self->sync_op);
    PyMutex_Lock(&self->accessors_lock);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        PyMutex_Lock(&storage->self_mutex);
    }
}

void
AtomicDict_EndSynchronousOperation(AtomicDict *self)
{
    PyMutex_Unlock(&self->sync_op);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        PyMutex_Unlock(&storage->self_mutex);
    }
    PyMutex_Unlock(&self->accessors_lock);
}

/**
 * put [entry; entry + n] into the buffer (inclusive). it may be that n = 1.
 * caller must ensure no segfaults, et similia.
 * */
void
AtomicDict_ReservationBufferPut(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc, int n, AtomicDictMeta *meta)
{
    // use asserts to check for circular buffer correctness (don't return and check for error)

    assert(n > 0);
    assert(n <= 64);

    for (int i = 0; i < n; ++i) {
        if (entry_loc->location + i == 0) {
            // entry 0 is reserved for correctness of tombstones
            continue;
        }
        assert(rb->count < 64);
        AtomicDictEntryLoc *head = &rb->reservations[rb->head];
        head->entry = AtomicDict_GetEntryAt(entry_loc->location + i, meta);
        head->location = entry_loc->location + i;
        assert(atomic_dict_entry_ix_sanity_check(head->location, meta));
        rb->head++;
        if (rb->head == 64) {
            rb->head = 0;
        }
        rb->count++;
    }
    assert(rb->count <= 64);
}

void
AtomicDict_ReservationBufferPop(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc)
{
    if (rb->count == 0) {
        entry_loc->entry = NULL;
        return;
    }

    AtomicDictEntryLoc *tail = &rb->reservations[rb->tail];
    entry_loc->entry = tail->entry;
    entry_loc->location = tail->location;
    memset(&rb->reservations[rb->tail], 0, sizeof(AtomicDictEntryLoc));
    rb->tail++;
    if (rb->tail == 64) {
        rb->tail = 0;
    }
    rb->count--;

    assert(rb->count >= 0);
}

void
AtomicDict_UpdatePagesInReservationBuffer(AtomicDictReservationBuffer *rb, uint64_t from_page, uint64_t to_page)
{
    for (int i = 0; i < rb->count; ++i) {
        AtomicDictEntryLoc *entry = &rb->reservations[(rb->tail + i) % RESERVATION_BUFFER_SIZE];

        if (entry == NULL)
            continue;

        if (AtomicDict_PageOf(entry->location) == from_page) {
            entry->location =
                AtomicDict_PositionInPageOf(entry->location) +
                (to_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE);
        }
    }
}

void
atomic_dict_accessor_len_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_len, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_len, new, memory_order_release);
    atomic_store_explicit((_Atomic (uint8_t) *) &self->len_dirty, 1, memory_order_release);
}

void
atomic_dict_accessor_inserted_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_inserted, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_inserted, new, memory_order_release);
}

void
atomic_dict_accessor_tombstones_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_tombstones, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_tombstones, new, memory_order_release);
}
