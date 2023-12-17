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

__attribute__((unused)) inline int64_t
AtomicIntHandle_Get(AtomicIntHandle *self)
{
    return AtomicInt_Get(self->integer);
}

inline PyObject *
AtomicIntHandle_Get_callable(AtomicIntHandle *self)
{
    return AtomicInt_Get_callable(self->integer);
}

__attribute__((unused)) inline void
AtomicIntHandle_Set(AtomicIntHandle *self, int64_t updated)
{
    return AtomicInt_Set(self->integer, updated);
}

inline PyObject *
AtomicIntHandle_Set_callable(AtomicIntHandle *self, PyObject *updated)
{
    return AtomicInt_Set_callable(self->integer, updated);
}

__attribute__((unused)) inline int
AtomicIntHandle_CompareAndSet(AtomicIntHandle *self, int64_t expected, int64_t updated)
{
    return AtomicInt_CompareAndSet(self->integer, expected, updated);
}

inline PyObject *
AtomicIntHandle_CompareAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_CompareAndSet_callable(self->integer, args, kwargs);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_GetAndSet(AtomicIntHandle *self, int64_t updated)
{
    return AtomicInt_GetAndSet(self->integer, updated);
}

inline PyObject *
AtomicIntHandle_GetAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_GetAndSet_callable(self->integer, args, kwargs);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_IncrementAndGet(AtomicIntHandle *self, int64_t other, int *overflow)
{
    return AtomicInt_IncrementAndGet(self->integer, other, overflow);
}

inline PyObject *
AtomicIntHandle_IncrementAndGet_callable(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_IncrementAndGet_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_GetAndIncrement(AtomicIntHandle *self, int64_t other, int *overflow)
{
    return AtomicInt_GetAndIncrement(self->integer, other, overflow);
}

inline PyObject *
AtomicIntHandle_GetAndIncrement_callable(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_GetAndIncrement_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_DecrementAndGet(AtomicIntHandle *self, int64_t other, int *overflow)
{
    return AtomicInt_DecrementAndGet(self->integer, other, overflow);
}

inline PyObject *
AtomicIntHandle_DecrementAndGet_callable(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_DecrementAndGet_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_GetAndDecrement(AtomicIntHandle *self, int64_t other, int *overflow)
{
    return AtomicInt_GetAndDecrement(self->integer, other, overflow);
}

inline PyObject *
AtomicIntHandle_GetAndDecrement_callable(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_GetAndDecrement_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_GetAndUpdate(AtomicIntHandle *self, PyObject *callable, int *overflow)
{
    return AtomicInt_GetAndUpdate(self->integer, callable, overflow);
}

inline PyObject *
AtomicIntHandle_GetAndUpdate_callable(AtomicIntHandle *self, PyObject *callable)
{
    return AtomicInt_GetAndUpdate_callable(self->integer, callable);
}

__attribute__((unused)) inline int64_t
AtomicIntHandle_UpdateAndGet(AtomicIntHandle *self, PyObject *callable, int *overflow)
{
    return AtomicInt_UpdateAndGet(self->integer, callable, overflow);
}

inline PyObject *
AtomicIntHandle_UpdateAndGet_callable(AtomicIntHandle *self, PyObject *callable)
{
    return AtomicInt_UpdateAndGet_callable(self->integer, callable);
}


inline PyObject *
AtomicIntHandle_GetHandle(AtomicIntHandle *self)
{
    return AtomicInt_GetHandle(self->integer);
}

inline Py_hash_t
AtomicIntHandle_Hash(AtomicIntHandle *self)
{
    return AtomicInt_Hash(self->integer);
}

inline PyObject *
AtomicIntHandle_Add(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Add(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Subtract(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Subtract(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Multiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Multiply(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Remainder(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Remainder(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Divmod(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Divmod(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Power(AtomicIntHandle *self, PyObject *other, PyObject *mod)
{
    return AtomicInt_Power(self->integer, other, mod);
}

inline PyObject *
AtomicIntHandle_Negative(AtomicIntHandle *self)
{
    return AtomicInt_Negative(self->integer);
}

inline PyObject *
AtomicIntHandle_Positive(AtomicIntHandle *self)
{
    return AtomicInt_Positive(self->integer);
}

inline PyObject *
AtomicIntHandle_Absolute(AtomicIntHandle *self)
{
    return AtomicInt_Absolute(self->integer);
}

inline int
AtomicIntHandle_Bool(AtomicIntHandle *self)
{
    return AtomicInt_Bool(self->integer);
}

inline PyObject *
AtomicIntHandle_Invert(AtomicIntHandle *self)
{
    return AtomicInt_Invert(self->integer);
}

inline PyObject *
AtomicIntHandle_Lshift(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Lshift(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Rshift(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Rshift(self->integer, other);
}

inline PyObject *
AtomicIntHandle_And(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_And(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Xor(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Xor(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Or(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_Or(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Int(AtomicIntHandle *self)
{
    return AtomicInt_Int(self->integer);
}

inline PyObject *
AtomicIntHandle_Float(AtomicIntHandle *self)
{
    return AtomicInt_Float(self->integer);
}

inline PyObject *
AtomicIntHandle_FloorDivide(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_FloorDivide(self->integer, other);
}

inline PyObject *
AtomicIntHandle_TrueDivide(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_TrueDivide(self->integer, other);
}

inline PyObject *
AtomicIntHandle_Index(AtomicIntHandle *self)
{
    return AtomicInt_Index(self->integer);
}


inline PyObject *
AtomicIntHandle_InplaceAdd(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceAdd_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceSubtract(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceSubtract_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceMultiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceMultiply_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceRemainder(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceRemainder_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplacePower(AtomicIntHandle *self, PyObject *other, PyObject *mod)
{
    return AtomicInt_InplacePower_internal(self->integer, other, mod, 0);
}

inline PyObject *
AtomicIntHandle_InplaceLshift(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceLshift_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceRshift(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceRshift_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceAnd(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceAnd_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceXor(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceXor_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceOr(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceOr_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceFloorDivide(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceFloorDivide_internal(self->integer, other, 0);
}

inline PyObject *
AtomicIntHandle_InplaceTrueDivide(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceTrueDivide(self->integer, other);
}

inline PyObject *
AtomicIntHandle_MatrixMultiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_MatrixMultiply(self->integer, other);
}

inline PyObject *
AtomicIntHandle_InplaceMatrixMultiply(AtomicIntHandle *self, PyObject *other)
{
    return AtomicInt_InplaceMatrixMultiply(self->integer, other);
}

inline PyObject *
AtomicIntHandle_RichCompare(AtomicIntHandle *self, PyObject *other, int op)
{
    return AtomicInt_RichCompare(self->integer, other, op);
}

inline PyObject *
AtomicIntHandle_AsIntegerRatio(AtomicIntHandle *self)
{
    return AtomicInt_AsIntegerRatio(self->integer);
}

inline PyObject *
AtomicIntHandle_BitLength(AtomicIntHandle *self)
{
    return AtomicInt_BitLength(self->integer);
}

inline PyObject *
AtomicIntHandle_Conjugate(AtomicIntHandle *self)
{
    return AtomicInt_Conjugate(self->integer);
}

inline PyObject *
AtomicIntHandle_FromBytes(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_FromBytes(self->integer, args, kwargs);
}

inline PyObject *
AtomicIntHandle_ToBytes(AtomicIntHandle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt_ToBytes(self->integer, args, kwargs);
}

inline PyObject *
AtomicIntHandle_Denominator_Get(AtomicIntHandle *self, void *closure)
{
    return AtomicInt_Denominator_Get(self->integer, closure);
}

inline PyObject *
AtomicIntHandle_Denominator_Set(AtomicIntHandle *self, PyObject *value, void *closure)
{
    return AtomicInt_Denominator_Set(self->integer, value, closure);
}

inline PyObject *
AtomicIntHandle_Numerator_Get(AtomicIntHandle *self, void *closure)
{
    return AtomicInt_Numerator_Get(self->integer, closure);
}

inline PyObject *
AtomicIntHandle_Numerator_Set(AtomicIntHandle *self, PyObject *value, void *closure)
{
    return AtomicInt_Numerator_Set(self->integer, value, closure);
}

inline PyObject *
AtomicIntHandle_Imag_Get(AtomicIntHandle *self, void *closure)
{
    return AtomicInt_Imag_Get(self->integer, closure);
}

inline PyObject *
AtomicIntHandle_Imag_Set(AtomicIntHandle *self, PyObject *value, void *closure)
{
    return AtomicInt_Imag_Set(self->integer, value, closure);
}

inline PyObject *
AtomicIntHandle_Real_Get(AtomicIntHandle *self, void *closure)
{
    return AtomicInt_Real_Get(self->integer, closure);
}

inline PyObject *
AtomicIntHandle_Real_Set(AtomicIntHandle *self, PyObject *value, void *closure)
{
    return AtomicInt_Real_Set(self->integer, value, closure);
}
