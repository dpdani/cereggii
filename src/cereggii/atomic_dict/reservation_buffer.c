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
        if (rb == NULL) {
            return NULL;
        }
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
        PyThread_tss_set(dk->tss_key, rb);
        int appended = PyList_Append(dk->reservation_buffers, (PyObject *) rb);
        if (appended == -1) {
            assert(rb != NULL);
            PyMem_Free(rb);
            return NULL;
        }
    }
    return rb;
}

/**
 * put [entry; entry + n] into the buffer (inclusive). it may be that n = 1.
 * caller must ensure no segfaults, et similia.
 * */
void
atomic_dict_reservation_buffer_put(atomic_dict_reservation_buffer *rb, atomic_dict_entry *entry, int n)
{
    // use asserts to check for circular buffer correctness (don't return and check for error)

    assert(n > 0);
    assert(n <= 64);

    for (int i = 0; i < n; ++i) {
        assert(rb->count < 64);
        rb->reservations[rb->head] = entry + i * sizeof(atomic_dict_entry);
        rb->head++;
        if (rb->head == 64) {
            rb->head = 0;
        }
        rb->count++;
    }
    assert(rb->count < 64);
}

void
atomic_dict_reservation_buffer_pop(atomic_dict_reservation_buffer *rb, atomic_dict_entry **entry)
{
    if (rb->count == 0) {
        *entry = NULL;
        return;
    }

    *entry = rb->reservations[rb->tail];
    rb->tail++;
    if (rb->tail == 64) {
        rb->tail = 0;
    }
    rb->count--;

    assert(rb->count >= 0);
}
