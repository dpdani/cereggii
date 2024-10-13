// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ref.h"
#include "atomic_ops.h"
#include "_internal_py_core.h"


PyObject *
AtomicRef_new(PyTypeObject *Py_UNUSED(type), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicRef *self;
    self = PyObject_GC_New(AtomicRef, &AtomicRef_Type);

    if (self == NULL)
        return NULL;

    self->reference = Py_None;

    PyObject_GC_Track(self);

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
#ifdef Py_GIL_DISABLED
        if (!_Py_IsImmortal(reference)) {
            _PyObject_SetMaybeWeakref(reference);
        }
#endif
        // decref'ed in destructor
        self->reference = reference;
    }
    return 0;

    fail:
//    Py_XDECREF(reference);
    return -1;
}

int
AtomicRef_traverse(AtomicRef *self, visitproc visit, void *arg)
{
    Py_VISIT(self->reference);
    return 0;
}

int
AtomicRef_clear(AtomicRef *self)
{
    Py_CLEAR(self->reference);

    return 0;
}

void AtomicRef_dealloc(AtomicRef *self)
{
    PyObject_GC_UnTrack(self);
    Py_CLEAR(self->reference);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


PyObject *AtomicRef_Get(AtomicRef *self)
{
    PyObject *reference;
    reference = self->reference;

#ifndef Py_GIL_DISABLED
    Py_INCREF(reference);
#else
    while (!_Py_TryIncref(reference)) {
        reference = self->reference;
    }
#endif
    return reference;
}

PyObject *
AtomicRef_Set(AtomicRef *self, PyObject *reference)
{
    assert(reference != NULL);

    Py_INCREF(reference);
#ifdef Py_GIL_DISABLED
    _PyObject_SetMaybeWeakref(reference);
#endif

    PyObject *current_reference;
    current_reference = AtomicRef_Get(self);
    while (!CereggiiAtomic_CompareExchangePtr((void **) &self->reference, current_reference, reference)) {
//        Py_DECREF(current_reference);
        current_reference = AtomicRef_Get(self);
    }

//    Py_DECREF(current_reference);  // decrement for the AtomicRef_Get
//    Py_DECREF(current_reference);  // decrement because not holding it anymore
    Py_RETURN_NONE;
}

int
AtomicRef_CompareAndSet(AtomicRef *self, PyObject *expected, PyObject *new)
{
    assert(expected != NULL);
    assert(new != NULL);

    Py_INCREF(new);
#ifdef Py_GIL_DISABLED
    if (!_Py_IsImmortal(new)) {
        _PyObject_SetMaybeWeakref(new);
    }
#endif
    int retval = CereggiiAtomic_CompareExchangePtr((void **) &self->reference, expected, new);
    if (retval) {
//        Py_DECREF(expected);
        return 1;
    } else {
//        Py_DECREF(new);
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
#ifdef Py_GIL_DISABLED
    if (!_Py_IsImmortal(new)) {
        _PyObject_SetMaybeWeakref(new);
    }
#endif
    PyObject *current_reference = CereggiiAtomic_ExchangePtr((void **) &self->reference, new);
    // don't decref current_reference: passing it to python
    return current_reference;
}
