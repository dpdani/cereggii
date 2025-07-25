// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_DICT_H
#define CEREGGII_ATOMIC_DICT_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "atomic_ref.h"


typedef struct AtomicDict {
    PyObject_HEAD

    AtomicRef *metadata;

    uint8_t min_log_size;
    uint8_t reservation_buffer_size;

    PyMutex sync_op;

    Py_ssize_t len;
    uint8_t len_dirty;

    Py_tss_t *accessor_key;
    PyMutex accessors_lock;
    struct AtomicDict_AccessorStorage *accessors;
} AtomicDict;

extern PyTypeObject AtomicDict_Type;

struct AtomicDict_FastIterator;
typedef struct AtomicDict_FastIterator AtomicDict_FastIterator;


PyObject *AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value);

PyObject *AtomicDict_GetItemOrDefaultVarargs(AtomicDict *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicDict_GetItem(AtomicDict *self, PyObject *key);

int AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value);

int AtomicDict_DelItem(AtomicDict *self, PyObject *key);

PyObject *AtomicDict_CompareAndSet(AtomicDict *self, PyObject *key, PyObject *expected, PyObject *desired);

PyObject *AtomicDict_CompareAndSet_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_Reduce(AtomicDict *self, PyObject *iterable, PyObject *aggregate);

PyObject *AtomicDict_Reduce_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceSum(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceSum_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceAnd(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceAnd_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceOr(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceOr_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceMax(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceMax_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceMin(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceMin_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceList(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceList_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_ReduceCount(AtomicDict *self, PyObject *iterable);

PyObject *AtomicDict_ReduceCount_callable(AtomicDict *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicDict_LenBounds(AtomicDict *self);

PyObject *AtomicDict_ApproxLen(AtomicDict *self);

Py_ssize_t AtomicDict_Len(AtomicDict *self);

Py_ssize_t AtomicDict_Len_impl(AtomicDict *self);

PyObject *AtomicDict_FastIter(AtomicDict *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicDict_BatchGetItem(AtomicDict *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicDict_GetHandle(AtomicDict *self);

PyObject *AtomicDict_Debug(AtomicDict *self);

PyObject *AtomicDict_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg);

int AtomicDict_clear(AtomicDict *self);

void AtomicDict_dealloc(AtomicDict *self);

PyObject *AtomicDict_ReHash(AtomicDict *self, PyObject *ob);


#endif //CEREGGII_ATOMIC_DICT_H
