// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict.h"


atomic_dict_reservation_buffer *
atomic_dict_get_reservation_buffer(AtomicDict *dk)
{
    assert(dk->tss_key != NULL);
    atomic_dict_reservation_buffer *rb = PyThread_tss_get(dk->tss_key);
    if (rb == NULL) {
        rb = PyObject_New(atomic_dict_reservation_buffer, &AtomicDictReservationBuffer);
        if (rb == NULL)
            return NULL;

        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
        int set = PyThread_tss_set(dk->tss_key, rb);
        if (set != 0)
            goto fail;

        int appended = PyList_Append(dk->reservation_buffers, (PyObject *) rb);
        if (appended == -1)
            goto fail;
    }

    return rb;
    fail:
    assert(rb != NULL);
    PyMem_Free(rb);
    return NULL;
}

/**
 * put [entry; entry + n] into the buffer (inclusive). it may be that n = 1.
 * caller must ensure no segfaults, et similia.
 * */
void
atomic_dict_reservation_buffer_put(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc, int n)
{
    // use asserts to check for circular buffer correctness (don't return and check for error)

    assert(n > 0);
    assert(n <= 64);

    for (int i = 0; i < n; ++i) {
        assert(rb->count < 64);
        atomic_dict_entry_loc *head = &rb->reservations[rb->head];
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
atomic_dict_reservation_buffer_pop(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc)
{
    if (rb->count == 0) {
        entry_loc->entry = NULL;
        return;
    }

    atomic_dict_entry_loc *tail = &rb->reservations[rb->tail];
    entry_loc->entry = tail->entry;
    entry_loc->location = tail->location;
    rb->tail++;
    if (rb->tail == 64) {
        rb->tail = 0;
    }
    rb->count--;

    assert(rb->count >= 0);
}
