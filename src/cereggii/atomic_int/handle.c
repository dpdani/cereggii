// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_int.h"
#include "atomic_int_internal.h"


PyObject *
AtomicIntHandle_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    AtomicIntHandle *self;
    self = (AtomicIntHandle *) type->tp_alloc(type, 0);

    return (PyObject *) self;
}

int
AtomicIntHandle_init(AtomicIntHandle *self, PyObject *args, PyObject *Py_UNUSED(kwargs))
{
    PyObject *integer = NULL;

    if (!PyArg_ParseTuple(args, "O", &integer))
        goto fail;

    assert(integer != NULL);
    Py_INCREF(integer);

    if (!PyObject_IsInstance(integer, (PyObject *) &AtomicInt_Type))
        goto fail;

    self->integer = (AtomicInt *) integer;

    return 0;

    fail:
    Py_XDECREF(integer);
    return -1;
}

void
AtomicIntHandle_dealloc(AtomicIntHandle *self)
{
    Py_XDECREF(self->integer);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

inline int64_t
AtomicIntHandle_Get(AtomicIntHandle *self)
{
    return AtomicInt_Get(self->integer);
}

inline PyObject *
AtomicIntHandle_Get_callable(AtomicIntHandle *self)
{
    return AtomicInt_Get_callable(self->integer);
}

inline void
AtomicIntHandle_Set(AtomicIntHandle *self, int64_t updated)
{
    return AtomicInt_Set(self->integer, updated);
}

inline PyObject *
AtomicIntHandle_Set_callable(AtomicIntHandle *self, PyObject *updated)
{
    return AtomicInt_Set_callable(self->integer, updated);
}

inline int
AtomicIntHandle_CompareAndSet(AtomicIntHandle *self, int64_t expected, int64_t updated)
{
    return AtomicInt_CompareAndSet(self->integer, expected, updated);
}

inline PyObject *
AtomicIntHandle_CompareAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_CompareAndSet_callable(self->integer, args, kwargs);
}

inline int64_t
AtomicIntHandle_GetAndSet(AtomicIntHandle *self, int64_t updated)
{
    return AtomicInt_GetAndSet(self->integer, updated);
}

inline PyObject *
AtomicIntHandle_GetAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_GetAndSet_callable(self->integer, args, kwargs);
}

inline PyObject *
AtomicIntHandle_GetHandle(AtomicIntHandle *self)
{
    return AtomicInt_GetHandle(self->integer);
}

inline PyObject *
AtomicIntHandle_Add(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Add(self->integer, other);
}

inline PyObject *
AtomicIntHandle_InplaceAdd(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceAdd_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_Subtract(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Subtract(self->integer, other);
}

inline PyObject *
AtomicIntHandle_InplaceSubtract(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceSubtract_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_Multiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Multiply(self->integer, other);
}

inline PyObject *
AtomicIntHandle_InplaceMultiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceMultiply_internal(self->integer, other, 0);
}
