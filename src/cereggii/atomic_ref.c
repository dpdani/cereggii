// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ref.h"
#include "atomic_ops.h"
#include "thread_handle.h"
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
AtomicRef_init(AtomicRef *self, PyObject *args, PyObject *kwargs)
{
    PyObject *initial_value = NULL;

    char *kw_list[] = {"initial_value", NULL};

    if (args != NULL) {
        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kw_list, &initial_value))
            goto fail;
    }

    if (initial_value != NULL) {
        _Py_SetWeakrefAndIncref(initial_value);
        // decref'ed in destructor
        self->reference = initial_value;
    }
    return 0;

    fail:
    Py_XDECREF(initial_value);
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
AtomicRef_Set(AtomicRef *self, PyObject *desired)
{
    assert(desired != NULL);

    _Py_SetWeakrefAndIncref(desired);

    PyObject *current_reference;
    current_reference = AtomicRef_Get(self);
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

    _Py_SetWeakrefAndIncref(desired);
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

PyObject *
AtomicRef_GetAndSet(AtomicRef *self, PyObject *desired)
{
    assert(desired != NULL);

    _Py_SetWeakrefAndIncref(desired);
    PyObject *current_reference = CereggiiAtomic_ExchangePtr((void **) &self->reference, desired);
    // don't decref current_reference: passing it to python
    return current_reference;
}

PyObject *
AtomicRef_GetHandle(AtomicRef *self)
{
    ThreadHandle *handle = NULL;

    handle = PyObject_GC_New(ThreadHandle, &ThreadHandle_Type);

    if (handle == NULL)
        goto fail;

    PyObject *args = Py_BuildValue("(O)", self);
    if (ThreadHandle_init(handle, args, NULL) < 0)
        goto fail;

    return (PyObject *) handle;

    fail:
    return NULL;
}
