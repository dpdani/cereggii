// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_H
#define CEREGGII_ATOMIC_INT_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>


typedef struct atomic_int {
    PyObject_HEAD

    int64_t integer;
} AtomicInt;

PyObject *AtomicInt_Get(AtomicInt *self);

void AtomicInt_Set(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_Set_callable(AtomicInt *self, PyObject *updated);

int AtomicInt_CompareAndSet(AtomicInt *self, int64_t expected, int64_t updated);

PyObject *AtomicInt_CompareAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);

int64_t AtomicInt_GetAndSet(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_GetAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);


PyObject *AtomicInt_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

int AtomicInt_init(AtomicInt *self, PyObject *args, PyObject *kwargs);

void AtomicInt_dealloc(AtomicInt *self);

extern PyTypeObject AtomicInt_Type;


#endif // CEREGGII_ATOMIC_INT_H
