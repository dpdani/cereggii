// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <stdatomic.h>

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "constants.h"
#include "_internal_py_core.h"


PyObject *
AtomicDict_FastIter(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    int partitions = 1, this_partition = 0;

    char *kw_list[] = {"partitions", "this_partition", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", kw_list, &partitions, &this_partition))
        goto fail_parse;

    if (partitions <= 0) {
        PyErr_SetString(PyExc_ValueError, "partitions <= 0");
        goto fail_parse;
    }

    if (this_partition > partitions) {
        PyErr_SetString(PyExc_ValueError, "this_partition > partitions");
        goto fail_parse;
    }

    AtomicDictFastIterator *iter = NULL;
    Py_INCREF(self);

    iter = PyObject_New(AtomicDictFastIterator, &AtomicDictFastIterator_Type);
    if (iter == NULL)
        goto fail;

    iter->meta = NULL;
    iter->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);
    if (iter->meta == NULL)
        goto fail;

    iter->dict = self;
    iter->position = this_partition * ATOMIC_DICT_ENTRIES_IN_PAGE;
    iter->partitions = partitions;

    return (PyObject *) iter;

    fail:
    Py_XDECREF(iter);
    Py_DECREF(self);
    fail_parse:
    return NULL;
}

void
AtomicDictFastIterator_dealloc(AtomicDictFastIterator *self)
{
    Py_CLEAR(self->dict);
    Py_CLEAR(self->meta);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *AtomicDictFastIterator_GetIter(AtomicDictFastIterator *self)
{
    Py_INCREF(self);
    return (PyObject *) self;
}

PyObject *
AtomicDictFastIterator_Next(AtomicDictFastIterator *self)
{
    AtomicDictEntry *entry_p, entry = {
        .value = NULL,
    };

    AtomicDictMeta *meta = self->meta;
    int64_t gap = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
    if (gap < 0) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    while (entry.value == NULL) {
        if (page_of(self->position) > (uint64_t) gap) {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }

        entry_p = get_entry_at(self->position, self->meta);
        read_entry(entry_p, &entry);

        // it doesn't seem to be worth it
//        if ((self->position & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1)) == 0
//            && page_of(self->position + ATOMIC_DICT_ENTRIES_IN_PAGE * 2) <= self->meta->greatest_allocated_block)
//        {
//            for (uint64_t i = self->position; i < self->position + ATOMIC_DICT_ENTRIES_IN_PAGE * 2; ++i) {
//                cereggii_prefetch(get_entry_at(i, self->meta), 0, 1);
//                // 0: the prefetch is for a read
//                // 1: the prefetched address is unlikely to be read again soon
//            }
//        }

        if (((self->position + 1) & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1)) == 0) {
            self->position = (self->position & ~(ATOMIC_DICT_ENTRIES_IN_PAGE - 1))
                + self->partitions * ATOMIC_DICT_ENTRIES_IN_PAGE;
        }
        else {
            self->position++;
        }
    }
    if (entry.key == NULL || !_Py_TryIncref(entry.key)) {
        goto concurrent_usage_detected;
    }
    if (entry.value == NULL || !_Py_TryIncref(entry.value)) {
        Py_DECREF(entry.key);
        goto concurrent_usage_detected;
    }
    return Py_BuildValue("(NN)", entry.key, entry.value);
    concurrent_usage_detected:
    PyErr_SetString(Cereggii_ConcurrentUsageDetected, "please see https://dpdani.github.io/cereggii/api/AtomicDict/#cereggii._cereggii.AtomicDict.fast_iter");
    return NULL;
}
