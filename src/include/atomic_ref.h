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

PyObject *atomic_ref_get_ref(AtomicRef *self);

PyObject *atomic_ref_set_ref(AtomicRef *self, PyObject *reference);

PyObject *atomic_ref_compare_and_set(AtomicRef *self, PyObject *expected, PyObject *new);

PyObject *atomic_ref_get_and_set(AtomicRef *self, PyObject *new);


PyObject *AtomicRef_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicRef_init(AtomicRef *self, PyObject *args, PyObject *kwargs);

void AtomicRef_dealloc(AtomicRef *self);

int AtomicRef_traverse(AtomicRef *self, visitproc visit, void *arg);

extern PyTypeObject AtomicRef_Type;


#endif // CEREGGII_ATOMIC_REF_H
