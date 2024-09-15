// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ref.h"
#include "atomic_ops.h"


PyObject *
AtomicRef_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
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
AtomicRef_init(AtomicRef *self, PyObject *args, PyObject *Py_UNUSED(kwargs))
{
    PyObject *reference = NULL;
    if (args != NULL) {
        if (!PyArg_ParseTuple(args, "|O", &reference))
            goto fail;
    }

    if (reference != NULL) {
        Py_INCREF(reference);
        // decref'ed in destructor
        self->reference = reference;
    }
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
AtomicRef_Set(AtomicRef *self, PyObject *desired)
{
    assert(desired != NULL);

    PyObject *current_reference;
    current_reference = AtomicRef_Get(self);
    Py_INCREF(desired);
    while (!CereggiiAtomic_CompareExchangePtr((void **) &self->reference, current_reference, desired)) {
        Py_DECREF(current_reference);
        current_reference = AtomicRef_Get(self);
    }
    Py_DECREF(current_reference);  // decrement for the AtomicRef_Get
    Py_DECREF(current_reference);  // decrement because not holding it anymore
    Py_RETURN_NONE;
}

int
AtomicRef_CompareAndSet(AtomicRef *self, PyObject *expected, PyObject *desired)
{
    assert(expected != NULL);
    assert(desired != NULL);

    Py_INCREF(desired);
    int retval = CereggiiAtomic_CompareExchangePtr((void **) &self->reference, expected, desired);
    if (retval) {
        Py_DECREF(expected);
        return 1;
    } else {
        Py_DECREF(desired);
        return 0;
    }
}

PyObject *
AtomicRef_CompareAndSet_callable(AtomicRef *self, PyObject *args, PyObject *kwargs)
{
    PyObject *expected = NULL;
    PyObject *desired = NULL;

    char *kw_list[] = {"expected", "desired", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kw_list, &expected, &desired)) {
        return NULL;
    }

    if (AtomicRef_CompareAndSet(self, expected, desired)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyObject *AtomicRef_GetAndSet(AtomicRef *self, PyObject *desired)
{
    assert(desired != NULL);

    Py_INCREF(desired);
    PyObject *current_reference = CereggiiAtomic_ExchangePtr((void **) &self->reference, desired);
    // don't decref current_reference: passing it to python
    return current_reference;
}
