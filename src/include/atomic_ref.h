// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_REF_H
#define CEREGGII_ATOMIC_REF_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>


typedef struct atomic_ref {
    PyObject_HEAD

    PyObject *reference;
} AtomicRef;

PyObject *AtomicRef_Get(AtomicRef *self);

PyObject *AtomicRef_Set(AtomicRef *self, PyObject *desired);

int AtomicRef_CompareAndSet(AtomicRef *self, PyObject *expected, PyObject *desired);

PyObject *AtomicRef_CompareAndSet_callable(AtomicRef *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicRef_GetAndSet(AtomicRef *self, PyObject *desired);

PyObject *AtomicRef_GetHandle(AtomicRef *self);


PyObject *AtomicRef_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicRef_init(AtomicRef *self, PyObject *args, PyObject *kwargs);

int AtomicRef_traverse(AtomicRef *self, visitproc visit, void *arg);

int AtomicRef_clear(AtomicRef *self);

void AtomicRef_dealloc(AtomicRef *self);

extern PyTypeObject AtomicRef_Type;


/// handle

typedef struct {
    PyObject_HEAD

    AtomicRef *ref;
} AtomicRefHandle;

PyObject *AtomicRefHandle_Get(AtomicRefHandle *self);

PyObject *AtomicRefHandle_Set(AtomicRefHandle *self, PyObject *desired);

int AtomicRefHandle_CompareAndSet(AtomicRefHandle *self, PyObject *expected, PyObject *desired);

PyObject *AtomicRefHandle_CompareAndSet_callable(AtomicRefHandle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicRefHandle_GetAndSet(AtomicRefHandle *self, PyObject *desired);

PyObject *AtomicRefHandle_GetHandle(AtomicRefHandle *self);


PyObject *AtomicRefHandle_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicRefHandle_init(AtomicRefHandle *self, PyObject *args, PyObject *kwargs);

int AtomicRefHandle_traverse(AtomicRefHandle *self, visitproc visit, void *arg);

int AtomicRefHandle_clear(AtomicRefHandle *self);

void AtomicRefHandle_dealloc(AtomicRefHandle *self);

extern PyTypeObject AtomicRefHandle_Type;


#endif // CEREGGII_ATOMIC_REF_H
