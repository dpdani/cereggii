// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ref.h"


PyObject *
AtomicRef_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AtomicRef *self;
    self = (AtomicRef *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->reference = Py_None;
    }
    if (!PyObject_GC_IsTracked((PyObject *) self)) {
        PyObject_GC_Track(self);
    }
    return (PyObject *) self;
}

int
AtomicRef_init(AtomicRef *self, PyObject *args, PyObject *kwargs)
{
    PyObject *reference = NULL;
    if (args != NULL) {
        if (!PyArg_ParseTuple(args, "|O", &reference))
            goto fail;
    }

    if (reference != NULL) {
        Py_INCREF(reference);
        self->reference = reference;
    }
    // decref'ed in destructor
    return 0;

    fail:
    Py_XDECREF(reference);
    return -1;
}

void AtomicRef_dealloc(AtomicRef *self)
{
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->reference);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

int
AtomicRef_traverse(AtomicRef *self, visitproc visit, void *arg)
{
    Py_VISIT(self->reference);
    return 0;
}


PyObject *AtomicRef_Get(AtomicRef *self)
{
    PyObject *reference;
    reference = self->reference;
    while (!_Py_TRY_INCREF(reference)) {
        reference = self->reference;
    }
    return reference;
}

PyObject *
AtomicRef_Set(AtomicRef *self, PyObject *reference)
{
    assert(reference != NULL);

    PyObject *current_reference;
    current_reference = AtomicRef_Get(self);
    Py_INCREF(reference);
    while (!_Py_atomic_compare_exchange_ptr(&self->reference, current_reference, reference)) {
        Py_DECREF(current_reference);
        current_reference = AtomicRef_Get(self);
    }
    Py_DECREF(current_reference);
    Py_RETURN_NONE;
}

int
AtomicRef_CompareAndSet(AtomicRef *self, PyObject *expected, PyObject *new)
{
    assert(expected != NULL);
    assert(new != NULL);

    Py_INCREF(new);
    int retval = _Py_atomic_compare_exchange_ptr(&self->reference, expected, new);
    if (retval) {
        Py_DECREF(expected);
        return 1;
    } else {
        Py_DECREF(new);
        return 0;
    }
}

PyObject *
AtomicRef_CompareAndSet_callable(AtomicRef *self, PyObject *args, PyObject *Py_UNUSED(kwargs))
{
    PyObject *expected;
    PyObject *new;

    if (!PyArg_ParseTuple(args, "OO", &expected, &new)) {
        return NULL;
    }

    if (AtomicRef_CompareAndSet(self, expected, new)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyObject *AtomicRef_GetAndSet(AtomicRef *self, PyObject *new)
{
    assert(new != NULL);

    Py_INCREF(new);
    PyObject *current_reference = _Py_atomic_exchange_ptr(&self->reference, new);
    // don't decref current_reference: passing it to python
    return current_reference;
}
