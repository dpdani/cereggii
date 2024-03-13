// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"


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

    AtomicDict_FastIterator *iter = NULL;
    Py_INCREF(self);

    iter = PyObject_New(AtomicDict_FastIterator, &AtomicDictFastIterator_Type);
    if (iter == NULL)
        goto fail;

    iter->meta = NULL;
    iter->meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (iter->meta == NULL)
        goto fail;

    iter->dict = self;
    iter->position = this_partition * ATOMIC_DICT_ENTRIES_IN_BLOCK;
    iter->partitions = partitions;

    return (PyObject *) iter;

    fail:
    Py_XDECREF(iter);
    Py_DECREF(self);
    fail_parse:
    return NULL;
}

void
AtomicDictFastIterator_dealloc(AtomicDict_FastIterator *self)
{
    Py_CLEAR(self->dict);
    Py_CLEAR(self->meta);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *AtomicDictFastIterator_GetIter(AtomicDict_FastIterator *self)
{
    Py_INCREF(self);
    return (PyObject *) self;
}

PyObject *
AtomicDictFastIterator_Next(AtomicDict_FastIterator *self)
{
    AtomicDict_Entry *entry_p, entry = {
        .value = NULL,
    };

    while (entry.value == NULL) {
        if (AtomicDict_BlockOf(self->position) > self->meta->greatest_allocated_block) {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }

        entry_p = AtomicDict_GetEntryAt(self->position, self->meta);
        AtomicDict_ReadEntry(entry_p, &entry);

        // it doesn't seem to be worth it
//        if ((self->position & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1)) == 0
//            && AtomicDict_BlockOf(self->position + ATOMIC_DICT_ENTRIES_IN_BLOCK * 2) <= self->meta->greatest_allocated_block)
//        {
//            for (uint64_t i = self->position; i < self->position + ATOMIC_DICT_ENTRIES_IN_BLOCK * 2; ++i) {
//                __builtin_prefetch(AtomicDict_GetEntryAt(i, self->meta), 0, 1);
//                // 0: the prefetch is for a read
//                // 1: the prefetched address is unlikely to be read again soon
//            }
//        }

        if (((self->position + 1) & (ATOMIC_DICT_ENTRIES_IN_BLOCK - 1)) == 0) {
            self->position = (self->position & ~(ATOMIC_DICT_ENTRIES_IN_BLOCK - 1))
                + self->partitions * ATOMIC_DICT_ENTRIES_IN_BLOCK;
        }
        else {
            self->position++;
        }
    }
    Py_INCREF(entry.key);
    Py_INCREF(entry.value);
    return Py_BuildValue("(OO)", entry.key, entry.value);
}
