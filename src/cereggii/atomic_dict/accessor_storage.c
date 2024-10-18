// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict.h"
#include "atomic_dict_internal.h"


AtomicDict_AccessorStorage *
AtomicDict_GetOrCreateAccessorStorage(AtomicDict *self)
{
    assert(self->accessor_key != NULL);
    AtomicDict_AccessorStorage *storage = PyThread_tss_get(self->accessor_key);

    if (storage == NULL) {
        storage = PyObject_New(AtomicDict_AccessorStorage, &AtomicDictAccessorStorage_Type);
        if (storage == NULL)
            return NULL;

        storage->self_mutex = (PyMutex) {0};
        storage->local_len = 0;
        storage->participant_in_migration = 0;
        storage->reservation_buffer.head = 0;
        storage->reservation_buffer.tail = 0;
        storage->reservation_buffer.count = 0;
        memset(storage->reservation_buffer.reservations, 0, sizeof(AtomicDict_EntryLoc) * RESERVATION_BUFFER_SIZE);

        int set = PyThread_tss_set(self->accessor_key, storage);
        if (set != 0)
            goto fail;

        int appended;
        PyMutex_Lock(&self->accessors_lock);
        appended = PyList_Append(self->accessors, (PyObject *) storage);
        PyMutex_Unlock(&self->accessors_lock);

        if (appended == -1)
            goto fail;
        Py_DECREF(storage);
    }

    return storage;
    fail:
    assert(storage != NULL);
    Py_DECREF(storage);
    return NULL;
}

AtomicDict_AccessorStorage *
AtomicDict_GetAccessorStorage(Py_tss_t *accessor_key)
{
    assert(accessor_key != NULL);
    AtomicDict_AccessorStorage *storage = PyThread_tss_get(accessor_key);
    assert(storage != NULL);
    return storage;
}

void
AtomicDict_BeginSynchronousOperation(AtomicDict *self)
{
    PyMutex_Lock(&self->sync_op);
    PyMutex_Lock(&self->accessors_lock);
    for (Py_ssize_t i = 0; i < PyList_Size(self->accessors); ++i) {
        AtomicDict_AccessorStorage *storage = NULL;
        storage = (AtomicDict_AccessorStorage *) PyList_GetItemRef(self->accessors, i);
        assert(storage != NULL);

        PyMutex_Lock(&storage->self_mutex);
    }
}

void
AtomicDict_EndSynchronousOperation(AtomicDict *self)
{
    PyMutex_Unlock(&self->sync_op);
    for (Py_ssize_t i = 0; i < PyList_Size(self->accessors); ++i) {
        AtomicDict_AccessorStorage *storage = NULL;
        storage = (AtomicDict_AccessorStorage *) PyList_GetItemRef(self->accessors, i);
        assert(storage != NULL);

        PyMutex_Unlock(&storage->self_mutex);
    }
    PyMutex_Unlock(&self->accessors_lock);
}

void
AtomicDict_AccessorStorage_dealloc(AtomicDict_AccessorStorage *self)
{
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/**
 * put [entry; entry + n] into the buffer (inclusive). it may be that n = 1.
 * caller must ensure no segfaults, et similia.
 * */
void
AtomicDict_ReservationBufferPut(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc, int n)
{
    // use asserts to check for circular buffer correctness (don't return and check for error)

    assert(n > 0);
    assert(n <= 64);

    for (int i = 0; i < n; ++i) {
        if (entry_loc->location + i == 0) { // entry 0 is forbidden (cf. reservations / tombstones)
            continue;
        }
        assert(rb->count < 64);
        AtomicDict_EntryLoc *head = &rb->reservations[rb->head];
        head->entry = entry_loc->entry + i;
        head->location = entry_loc->location + i;
        rb->head++;
        if (rb->head == 64) {
            rb->head = 0;
        }
        rb->count++;
    }
    assert(rb->count <= 64);
}

void
AtomicDict_ReservationBufferPop(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc)
{
    if (rb->count == 0) {
        entry_loc->entry = NULL;
        return;
    }

    AtomicDict_EntryLoc *tail = &rb->reservations[rb->tail];
    entry_loc->entry = tail->entry;
    entry_loc->location = tail->location;
    memset(&rb->reservations[rb->tail], 0, sizeof(AtomicDict_EntryLoc));
    rb->tail++;
    if (rb->tail == 64) {
        rb->tail = 0;
    }
    rb->count--;

    assert(rb->count >= 0);
}

void
AtomicDict_UpdateBlocksInReservationBuffer(AtomicDict_ReservationBuffer *rb, uint64_t from_block, uint64_t to_block)
{
    for (int i = 0; i < rb->count; ++i) {
        AtomicDict_EntryLoc *entry = &rb->reservations[(rb->tail + i) % RESERVATION_BUFFER_SIZE];

        if (entry == NULL)
            continue;

        if (AtomicDict_BlockOf(entry->location) == from_block) {
            entry->location =
                AtomicDict_PositionInBlockOf(entry->location) +
                (to_block << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK);
        }
    }
}
