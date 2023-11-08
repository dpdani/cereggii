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


PyObject *atomic_ref_get_ref(AtomicRef *self)
{
    PyObject *reference;
    reference = self->reference;
    while (!_Py_TRY_INCREF(reference)) {
        reference = self->reference;
    }
    return reference;
}

PyObject *
atomic_ref_set_ref(AtomicRef *self, PyObject *reference)
{
    assert(reference != NULL);

    PyObject *current_reference;
    current_reference = atomic_ref_get_ref(self);
    Py_INCREF(reference);
    while (!_Py_atomic_compare_exchange_ptr(&self->reference, current_reference, reference)) {
        Py_DECREF(current_reference);
        current_reference = atomic_ref_get_ref(self);
    }
    Py_DECREF(current_reference);
    Py_RETURN_NONE;
}

PyObject *
atomic_ref_compare_and_set(AtomicRef *self, PyObject *args, PyObject *kwargs)
{
    PyObject *expected;
    PyObject *new;

    if (!PyArg_ParseTuple(args, "OO", &expected, &new)) {
        return 0;
    }

    assert(expected != NULL);
    assert(new != NULL);

    Py_INCREF(new);
    int retval = _Py_atomic_compare_exchange_ptr(&self->reference, expected, new);
    if (retval) {
        Py_DECREF(expected);
        Py_RETURN_TRUE;
    } else {
        Py_DECREF(new);
        Py_RETURN_FALSE;
    }
}

PyObject *atomic_ref_get_and_set(AtomicRef *self, PyObject *new)
{
    assert(new != NULL);

    Py_INCREF(new);
    PyObject *current_reference = _Py_atomic_exchange_ptr(&self->reference, new);
    // don't decref current_reference: passing it to python
    return current_reference;
}
