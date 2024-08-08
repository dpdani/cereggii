// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ref.h"
#include "atomic_ops.h"


static inline int
_Py_TryIncRefShared(PyObject *op)
{
    // I know, I know

    // https://github.com/python/cpython/blob/9e551f9b351440ebae79e07a02d0e4a1b61d139e/Include/internal/pycore_object.h#L499
    Py_ssize_t shared = op->ob_ref_shared;
    for (;;) {
        // If the shared refcount is zero and the object is either merged
        // or may not have weak references, then we cannot incref it.
        if (shared == 0 || shared == 0x3) {
            return 0;
        }

        if (CereggiiAtomic_CompareExchangeSsize(
            (Py_ssize_t *) &op->ob_ref_shared,
                shared,
                shared + (1 << _Py_REF_SHARED_SHIFT))) {
            return 1;
        }
        return 0;  // are they actually using some other function?
    }
}

static inline void
_PyObject_SetMaybeWeakref(PyObject *op)
{
    // https://github.com/python/cpython/blob/9e551f9b351440ebae79e07a02d0e4a1b61d139e/Include/internal/pycore_object.h#L608
    for (;;) {
        Py_ssize_t shared = CereggiiAtomic_LoadSsize((const Py_ssize_t *) &op->ob_ref_shared);
        if ((shared & 0x3) != 0) {
            // Nothing to do if it's in WEAKREFS, QUEUED, or MERGED states.
            return;
        }
        if (CereggiiAtomic_CompareExchangeSsize(
            (Py_ssize_t *) &op->ob_ref_shared, shared, shared | 0x1)) {
            return;
        }
    }
}


PyObject *
AtomicRef_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicRef *self;
    self = (AtomicRef *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->reference = Py_None;
        _PyObject_SetMaybeWeakref(self->reference);
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
        _PyObject_SetMaybeWeakref(reference);
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
    while (!_Py_TryIncRefShared(reference)) {
        reference = self->reference;
    }
    return reference;
}

PyObject *
AtomicRef_Set(AtomicRef *self, PyObject *reference)
{
    assert(reference != NULL);

    Py_INCREF(reference);
    _PyObject_SetMaybeWeakref(reference);
    PyObject *current_reference;
    current_reference = AtomicRef_Get(self);
    while (!CereggiiAtomic_CompareExchangePtr((void **) &self->reference, current_reference, reference)) {
        Py_DECREF(current_reference);
        current_reference = AtomicRef_Get(self);
    }
    Py_DECREF(current_reference);  // decrement for the AtomicRef_Get
    Py_DECREF(current_reference);  // decrement because not holding it anymore
    Py_RETURN_NONE;
}

int
AtomicRef_CompareAndSet(AtomicRef *self, PyObject *expected, PyObject *new)
{
    assert(expected != NULL);
    assert(new != NULL);

    Py_INCREF(new);
    _PyObject_SetMaybeWeakref(new);
    int retval = CereggiiAtomic_CompareExchangePtr((void **) &self->reference, expected, new);
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
    _PyObject_SetMaybeWeakref(new);
    PyObject *current_reference = CereggiiAtomic_ExchangePtr((void **) &self->reference, new);
    // don't decref current_reference: passing it to python
    return current_reference;
}
