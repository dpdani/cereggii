// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <stdatomic.h>

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


AtomicDict_Meta *
AtomicDictMeta_New(uint8_t log_size)
{
    void *generation = NULL;
    uint64_t *index = NULL;
    AtomicDict_Meta *meta = NULL;

    generation = PyMem_RawMalloc(1);
    if (generation == NULL)
        goto fail;

    index = PyMem_RawMalloc(sizeof(uint64_t) * (1ull << log_size));
    if (index == NULL)
        goto fail;

    meta = PyObject_GC_New(AtomicDict_Meta, &AtomicDictMeta_Type);
    if (meta == NULL)
        goto fail;

    meta->pages = NULL;
    meta->greatest_allocated_page = -1;
    meta->greatest_deleted_page = -1;
    meta->greatest_refilled_page = -1;
    meta->inserting_page = -1;

    meta->log_size = log_size;
    meta->generation = generation;
    meta->index = index;

    meta->new_gen_metadata = NULL;
    meta->migration_leader = 0;
    meta->node_to_migrate = 0;
    meta->accessor_key = NULL;
    meta->participants = NULL;

    meta->new_metadata_ready = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->new_metadata_ready == NULL)
        goto fail;
    meta->node_migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->node_migration_done == NULL)
        goto fail;
    meta->migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->migration_done == NULL)
        goto fail;

    PyObject_GC_Track(meta);
    return meta;
    fail:
    if (generation != NULL) {
        PyMem_RawFree(generation);
    }
    Py_XDECREF(meta);
    if (index != NULL) {
        PyMem_RawFree(index);
    }
    return NULL;
}

void
AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta)
{
    memset(meta->index, 0, sizeof(uint64_t) * SIZE_OF(meta));
}

int
AtomicDictMeta_InitPages(AtomicDict_Meta *meta)
{
    AtomicDict_Page **pages = NULL;
    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    pages = PyMem_RawMalloc(sizeof(AtomicDict_Page *) * (SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE));
    if (pages == NULL)
        goto fail;

    pages[0] = NULL;
    meta->pages = pages;
    meta->inserting_page = -1;
    meta->greatest_allocated_page = -1;
    meta->greatest_deleted_page = -1;
    meta->greatest_refilled_page = -1;

    return 0;

    fail:
    return -1;
}

int
AtomicDictMeta_CopyPages(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    assert(from_meta != NULL);
    assert(to_meta != NULL);
    assert(from_meta->log_size <= to_meta->log_size);

    AtomicDict_Page **previous_pages = from_meta->pages;
    int64_t inserting_page = from_meta->inserting_page;
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &from_meta->greatest_allocated_page, memory_order_acquire);
    int64_t greatest_deleted_page = from_meta->greatest_deleted_page;
    int64_t greatest_refilled_page = from_meta->greatest_refilled_page;


    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    AtomicDict_Page **pages = PyMem_RawMalloc(sizeof(AtomicDict_Page *) *
                                                (SIZE_OF(to_meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE));
    if (pages == NULL)
        goto fail;

    if (previous_pages != NULL) {
        for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
            pages[page_i] = previous_pages[page_i];
        }
    }

    if (previous_pages == NULL) {
        pages[0] = NULL;
    } else {
        pages[greatest_allocated_page + 1] = NULL;
    }

    to_meta->pages = pages;

    to_meta->inserting_page = inserting_page;
    atomic_store_explicit((_Atomic (int64_t) *) &to_meta->greatest_allocated_page, greatest_allocated_page, memory_order_release);
    to_meta->greatest_deleted_page = greatest_deleted_page;
    to_meta->greatest_refilled_page = greatest_refilled_page;

    return 1;

    fail:
    return -1;
}

void
AtomicDictMeta_ShrinkPages(AtomicDict *self, AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta)
{
    to_meta->pages[0] = from_meta->pages[0];  // entry 0 must be kept

    int64_t page_j = 1;
    for (int64_t page_i = 1; page_i <= from_meta->greatest_allocated_page; ++page_i) {
        if (from_meta->greatest_refilled_page < page_i && page_i <= from_meta->greatest_deleted_page)
            continue;

        to_meta->pages[page_j] = from_meta->pages[page_i];

        AtomicDict_AccessorStorage *storage;
        FOR_EACH_ACCESSOR(self, storage) {
            AtomicDict_UpdatePagesInReservationBuffer(&storage->reservation_buffer, page_i, page_j);
        }

        page_j++;
    }
    page_j--;

    to_meta->inserting_page = page_j;
    to_meta->greatest_allocated_page = page_j;

    if (from_meta->greatest_refilled_page > 0) {
        to_meta->greatest_refilled_page = 0;
    } else {
        to_meta->greatest_refilled_page = -1;
    }

    if (from_meta->greatest_deleted_page > 0) {
        to_meta->greatest_deleted_page = 0;
    } else {
        to_meta->greatest_deleted_page = -1;
    }
}

int
AtomicDictMeta_traverse(AtomicDict_Meta *self, visitproc visit, void *arg)
{
    Py_VISIT(self->new_gen_metadata);
    Py_VISIT(self->new_metadata_ready);
    Py_VISIT(self->node_migration_done);
    Py_VISIT(self->migration_done);

    if (self->pages == NULL)
        return 0;

    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_VISIT(self->pages[page_i]);
    }
    return 0;
}

int
AtomicDictMeta_clear(AtomicDict_Meta *self)
{
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_CLEAR(self->pages[page_i]);
    }

    Py_CLEAR(self->new_gen_metadata);
    Py_CLEAR(self->new_metadata_ready);
    Py_CLEAR(self->node_migration_done);
    Py_CLEAR(self->migration_done);

    return 0;
}

void
AtomicDictMeta_dealloc(AtomicDict_Meta *self)
{
    PyObject_GC_UnTrack(self);

    AtomicDictMeta_clear(self);

    uint64_t *index = self->index;
    if (index != NULL) {
        self->index = NULL;
        PyMem_RawFree(index);
    }

    if (self->pages != NULL) {
        PyMem_RawFree(self->pages);
    }

    if (self->participants != NULL) {
        PyMem_RawFree(self->participants);
    }

    PyMem_RawFree(self->generation);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
