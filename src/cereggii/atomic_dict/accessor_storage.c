// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <cereggii/atomic_dict.h>
#include <cereggii/atomic_dict_internal.h>
#include <stdatomic.h>
#include <cereggii/_internal_py_core.h>


AtomicDictAccessorStorage *
get_or_create_accessor_storage(AtomicDict *self)
{
    assert(self->accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(self->accessor_key);

    if (storage == NULL) {
        storage = PyMem_RawMalloc(sizeof(AtomicDictAccessorStorage));
        if (storage == NULL) {
            PyErr_NoMemory();
            return NULL;
        }

        *storage = (AtomicDictAccessorStorage) {0};
        // don't need to initialize with atomics because the PyMutex_Lock
        // below is already a seq-cst memory barrier
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
get_accessor_storage(Py_tss_t *accessor_key)
{
    assert(accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(accessor_key);
    assert(storage != NULL);
    return storage;
}

void
free_accessor_storage(AtomicDictAccessorStorage *self)
{
    Py_CLEAR(self->meta);
    PyMem_RawFree(self);
}

void
free_accessor_storage_list(AtomicDictAccessorStorage *head)
{
    if (head == NULL)
        return;

    AtomicDictAccessorStorage *prev = head;
    AtomicDictAccessorStorage *next = head->next_accessor;

    while (next) {
        free_accessor_storage(prev);
        prev = next;
        next = next->next_accessor;
    }

    free_accessor_storage(prev);
}

AtomicDictMeta *
get_meta(AtomicDict *self, AtomicDictAccessorStorage *storage)
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
begin_synchronous_operation(AtomicDict *self)
{
    PyMutex_Lock(&self->sync_op);
    PyMutex_Lock(&self->accessors_lock);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        PyMutex_Lock(&storage->self_mutex);
    }
}

void
end_synchronous_operation(AtomicDict *self)
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
reservation_buffer_put(AtomicDictReservationBuffer *rb, uint64_t location, uint8_t size, AtomicDictMeta *meta)
{
    // use asserts to check for correctness (don't return and check for error)

    assert(size > 0);
    assert(size <= 64);
    assert(rb->size == rb->used);  // don't waste space
    assert(atomic_dict_entry_ix_sanity_check(location, meta));
    assert(atomic_dict_entry_ix_sanity_check(location + size - 1, meta));
    cereggii_unused_in_release_build(meta);

    rb->start = location;
    rb->size = size;
    rb->used = 0;
}

void
reservation_buffer_put_back_one(AtomicDictReservationBuffer *rb)
{
    assert(rb->used > 0);
    rb->used--;
}

void
reservation_buffer_pop(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc, AtomicDictMeta *meta)
{
    if (rb->used == rb->size) {
        entry_loc->entry = NULL;
        return;
    }

    entry_loc->location = rb->start + rb->used;
    entry_loc->entry = get_entry_at(entry_loc->location, meta);
    rb->used++;
}

void
accessor_len_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_len, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_len, new, memory_order_release);
    atomic_store_explicit((_Atomic (uint8_t) *) &self->len_dirty, 1, memory_order_release);
}

void
accessor_inserted_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_inserted, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_inserted, new, memory_order_release);
}

void
accessor_tombstones_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_tombstones, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_tombstones, new, memory_order_release);
}

int
lock_accessor_storage_or_help_resize(AtomicDict *self, AtomicDictAccessorStorage *storage, AtomicDictMeta *meta)
{
    // returns whether a resize happened
    if (!_PyMutex_TryLock(&storage->self_mutex)) {
        if (maybe_help_resize(self, meta, NULL)) {
            // resized
            return 1;
        }
        PyMutex_Lock(&storage->self_mutex);
    }
    return maybe_help_resize(self, meta, &storage->self_mutex);
}
