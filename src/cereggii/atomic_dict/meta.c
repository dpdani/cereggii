// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <stdatomic.h>

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


AtomicDictMeta *
AtomicDictMeta_New(uint8_t log_size)
{
    uint64_t *index = NULL;
    AtomicDictMeta *meta = NULL;

    index = PyMem_RawMalloc(sizeof(uint64_t) * (1ull << log_size));
    if (index == NULL)
        goto fail;

    meta = PyObject_GC_New(AtomicDictMeta, &AtomicDictMeta_Type);
    if (meta == NULL)
        goto fail;

    meta->pages = NULL;
    meta->greatest_allocated_page = -1;

    meta->log_size = log_size;
    meta->index = index;

    meta->new_gen_metadata = NULL;
    meta->resize_leader = 0;
    meta->node_to_migrate = 0;
    meta->participants = NULL;

    meta->new_metadata_ready = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->new_metadata_ready == NULL)
        goto fail;
    meta->node_migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->node_migration_done == NULL)
        goto fail;
    meta->resize_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->resize_done == NULL)
        goto fail;

    PyObject_GC_Track(meta);
    return meta;
    fail:
    Py_XDECREF(meta);
    if (index != NULL) {
        PyMem_RawFree(index);
    }
    return NULL;
}

void
meta_clear_index(AtomicDictMeta *meta)
{
    memset(meta->index, 0, sizeof(uint64_t) * SIZE_OF(meta));
}

int
meta_init_pages(AtomicDictMeta *meta)
{
    AtomicDictPage **pages = NULL;
    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    pages = PyMem_RawMalloc(sizeof(AtomicDictPage *) * (SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE));
    if (pages == NULL)
        goto fail;

    pages[0] = NULL;
    meta->pages = pages;
    meta->greatest_allocated_page = -1;

    return 0;

    fail:
    return -1;
}

int
meta_copy_pages(AtomicDictMeta *from_meta, AtomicDictMeta *to_meta)
{
    assert(from_meta != NULL);
    assert(to_meta != NULL);
    assert(from_meta->log_size <= to_meta->log_size);

    AtomicDictPage **previous_pages = from_meta->pages;
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &from_meta->greatest_allocated_page, memory_order_acquire);

    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    AtomicDictPage **pages = PyMem_RawMalloc(sizeof(AtomicDictPage *) *
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
    atomic_store_explicit((_Atomic (int64_t) *) &to_meta->greatest_allocated_page, greatest_allocated_page, memory_order_release);

    return 1;

    fail:
    return -1;
}

int
AtomicDictMeta_traverse(AtomicDictMeta *self, visitproc visit, void *arg)
{
    Py_VISIT(self->new_gen_metadata);
    Py_VISIT(self->new_metadata_ready);
    Py_VISIT(self->node_migration_done);
    Py_VISIT(self->resize_done);

    if (self->pages == NULL)
        return 0;

    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_VISIT(self->pages[page_i]);
    }
    return 0;
}

int
AtomicDictMeta_clear(AtomicDictMeta *self)
{
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_CLEAR(self->pages[page_i]);
    }

    Py_CLEAR(self->new_gen_metadata);
    Py_CLEAR(self->new_metadata_ready);
    Py_CLEAR(self->node_migration_done);
    Py_CLEAR(self->resize_done);

    return 0;
}

void
AtomicDictMeta_dealloc(AtomicDictMeta *self)
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

    Py_TYPE(self)->tp_free((PyObject *) self);
}
