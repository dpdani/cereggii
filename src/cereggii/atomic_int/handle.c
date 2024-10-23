// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_int.h"
#include "atomic_int_internal.h"

int
AtomicInt64Handle_init(AtomicInt64Handle *self, PyObject *args, PyObject *Py_UNUSED(kwargs))
{
    PyObject *integer = NULL;

    if (!PyArg_ParseTuple(args, "O", &integer))
        goto fail;

    assert(integer != NULL);
    Py_INCREF(integer);

    if (!PyObject_IsInstance(integer, (PyObject *) &AtomicInt64_Type))
        goto fail;

    self->integer = (AtomicInt64 *) integer;
//    CereggiiAtomic_StorePtr((void **) &self->integer, integer);

    return 0;

    fail:
    Py_XDECREF(integer);
    return -1;
}

void
AtomicInt64Handle_dealloc(AtomicInt64Handle *self)
{
    Py_XDECREF(self->integer);
    Py_TYPE(self)->tp_free((PyObject *) self);

//    CereggiiAtomic_FenceRelease();
//    PyObject *integer = NULL;
//    integer = CereggiiAtomic_LoadPtr((const void **) &self->integer);
//    if (integer != NULL) {
//        CereggiiAtomic_StorePtr((void **) &self->integer, NULL);
//        Py_DECREF(integer);
//    }
//    CereggiiAtomic_FenceRelease();
//    Py_TYPE(self)->tp_free((PyObject *) self);
//    CereggiiAtomic_FenceRelease();
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_Get(AtomicInt64Handle *self)
{
    return AtomicInt64_Get(self->integer);
}

inline PyObject *
AtomicInt64Handle_Get_callable(AtomicInt64Handle *self)
{
    return AtomicInt64_Get_callable(self->integer);
}

__attribute__((unused)) inline void
AtomicInt64Handle_Set(AtomicInt64Handle *self, int64_t updated)
{
    return AtomicInt64_Set(self->integer, updated);
}

inline PyObject *
AtomicInt64Handle_Set_callable(AtomicInt64Handle *self, PyObject *updated)
{
    return AtomicInt64_Set_callable(self->integer, updated);
}

__attribute__((unused)) inline int
AtomicInt64Handle_CompareAndSet(AtomicInt64Handle *self, int64_t expected, int64_t updated)
{
    return AtomicInt64_CompareAndSet(self->integer, expected, updated);
}

inline PyObject *
AtomicInt64Handle_CompareAndSet_callable(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt64_CompareAndSet_callable(self->integer, args, kwargs);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_GetAndSet(AtomicInt64Handle *self, int64_t updated)
{
    return AtomicInt64_GetAndSet(self->integer, updated);
}

inline PyObject *
AtomicInt64Handle_GetAndSet_callable(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt64_GetAndSet_callable(self->integer, args, kwargs);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_IncrementAndGet(AtomicInt64Handle *self, int64_t other, int *overflow)
{
    return AtomicInt64_IncrementAndGet(self->integer, other, overflow);
}

inline PyObject *
AtomicInt64Handle_IncrementAndGet_callable(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_IncrementAndGet_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_GetAndIncrement(AtomicInt64Handle *self, int64_t other, int *overflow)
{
    return AtomicInt64_GetAndIncrement(self->integer, other, overflow);
}

inline PyObject *
AtomicInt64Handle_GetAndIncrement_callable(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_GetAndIncrement_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_DecrementAndGet(AtomicInt64Handle *self, int64_t other, int *overflow)
{
    return AtomicInt64_DecrementAndGet(self->integer, other, overflow);
}

inline PyObject *
AtomicInt64Handle_DecrementAndGet_callable(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_DecrementAndGet_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_GetAndDecrement(AtomicInt64Handle *self, int64_t other, int *overflow)
{
    return AtomicInt64_GetAndDecrement(self->integer, other, overflow);
}

inline PyObject *
AtomicInt64Handle_GetAndDecrement_callable(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_GetAndDecrement_callable(self->integer, other);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_GetAndUpdate(AtomicInt64Handle *self, PyObject *callable, int *overflow)
{
    return AtomicInt64_GetAndUpdate(self->integer, callable, overflow);
}

inline PyObject *
AtomicInt64Handle_GetAndUpdate_callable(AtomicInt64Handle *self, PyObject *callable)
{
    return AtomicInt64_GetAndUpdate_callable(self->integer, callable);
}

__attribute__((unused)) inline int64_t
AtomicInt64Handle_UpdateAndGet(AtomicInt64Handle *self, PyObject *callable, int *overflow)
{
    return AtomicInt64_UpdateAndGet(self->integer, callable, overflow);
}

inline PyObject *
AtomicInt64Handle_UpdateAndGet_callable(AtomicInt64Handle *self, PyObject *callable)
{
    return AtomicInt64_UpdateAndGet_callable(self->integer, callable);
}


inline PyObject *
AtomicInt64Handle_GetHandle(AtomicInt64Handle *self)
{
    return AtomicInt64_GetHandle(self->integer);
}

inline Py_hash_t
AtomicInt64Handle_Hash(AtomicInt64Handle *self)
{
    return AtomicInt64_Hash(self->integer);
}

inline PyObject *
AtomicInt64Handle_Add(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Add(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Subtract(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Subtract(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Multiply(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Multiply(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Remainder(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Remainder(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Divmod(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Divmod(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Power(AtomicInt64Handle *self, PyObject *other, PyObject *mod)
{
    return AtomicInt64_Power(self->integer, other, mod);
}

inline PyObject *
AtomicInt64Handle_Negative(AtomicInt64Handle *self)
{
    return AtomicInt64_Negative(self->integer);
}

inline PyObject *
AtomicInt64Handle_Positive(AtomicInt64Handle *self)
{
    return AtomicInt64_Positive(self->integer);
}

inline PyObject *
AtomicInt64Handle_Absolute(AtomicInt64Handle *self)
{
    return AtomicInt64_Absolute(self->integer);
}

inline int
AtomicInt64Handle_Bool(AtomicInt64Handle *self)
{
    return AtomicInt64_Bool(self->integer);
}

inline PyObject *
AtomicInt64Handle_Invert(AtomicInt64Handle *self)
{
    return AtomicInt64_Invert(self->integer);
}

inline PyObject *
AtomicInt64Handle_Lshift(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Lshift(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Rshift(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Rshift(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_And(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_And(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Xor(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Xor(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Or(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_Or(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Int(AtomicInt64Handle *self)
{
    return AtomicInt64_Int(self->integer);
}

inline PyObject *
AtomicInt64Handle_Float(AtomicInt64Handle *self)
{
    return AtomicInt64_Float(self->integer);
}

inline PyObject *
AtomicInt64Handle_FloorDivide(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_FloorDivide(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_TrueDivide(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_TrueDivide(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_Index(AtomicInt64Handle *self)
{
    return AtomicInt64_Index(self->integer);
}


inline PyObject *
AtomicInt64Handle_InplaceAdd(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceAdd_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceSubtract(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceSubtract_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceMultiply(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceMultiply_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceRemainder(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceRemainder_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplacePower(AtomicInt64Handle *self, PyObject *other, PyObject *mod)
{
    return AtomicInt64_InplacePower_internal(self->integer, other, mod, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceLshift(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceLshift_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceRshift(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceRshift_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceAnd(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceAnd_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceXor(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceXor_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceOr(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceOr_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceFloorDivide(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceFloorDivide_internal(self->integer, other, 0);
}

inline PyObject *
AtomicInt64Handle_InplaceTrueDivide(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceTrueDivide(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_MatrixMultiply(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_MatrixMultiply(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_InplaceMatrixMultiply(AtomicInt64Handle *self, PyObject *other)
{
    return AtomicInt64_InplaceMatrixMultiply(self->integer, other);
}

inline PyObject *
AtomicInt64Handle_RichCompare(AtomicInt64Handle *self, PyObject *other, int op)
{
    return AtomicInt64_RichCompare(self->integer, other, op);
}

inline PyObject *
AtomicInt64Handle_AsIntegerRatio(AtomicInt64Handle *self)
{
    return AtomicInt64_AsIntegerRatio(self->integer);
}

inline PyObject *
AtomicInt64Handle_BitLength(AtomicInt64Handle *self)
{
    return AtomicInt64_BitLength(self->integer);
}

inline PyObject *
AtomicInt64Handle_Conjugate(AtomicInt64Handle *self)
{
    return AtomicInt64_Conjugate(self->integer);
}

inline PyObject *
AtomicInt64Handle_FromBytes(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt64_FromBytes(self->integer, args, kwargs);
}

inline PyObject *
AtomicInt64Handle_ToBytes(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs)
{
    return AtomicInt64_ToBytes(self->integer, args, kwargs);
}

inline PyObject *
AtomicInt64Handle_Denominator_Get(AtomicInt64Handle *self, void *closure)
{
    return AtomicInt64_Denominator_Get(self->integer, closure);
}

inline PyObject *
AtomicInt64Handle_Denominator_Set(AtomicInt64Handle *self, PyObject *value, void *closure)
{
    return AtomicInt64_Denominator_Set(self->integer, value, closure);
}

inline PyObject *
AtomicInt64Handle_Numerator_Get(AtomicInt64Handle *self, void *closure)
{
    return AtomicInt64_Numerator_Get(self->integer, closure);
}

inline PyObject *
AtomicInt64Handle_Numerator_Set(AtomicInt64Handle *self, PyObject *value, void *closure)
{
    return AtomicInt64_Numerator_Set(self->integer, value, closure);
}

inline PyObject *
AtomicInt64Handle_Imag_Get(AtomicInt64Handle *self, void *closure)
{
    return AtomicInt64_Imag_Get(self->integer, closure);
}

inline PyObject *
AtomicInt64Handle_Imag_Set(AtomicInt64Handle *self, PyObject *value, void *closure)
{
    return AtomicInt64_Imag_Set(self->integer, value, closure);
}

inline PyObject *
AtomicInt64Handle_Real_Get(AtomicInt64Handle *self, void *closure)
{
    return AtomicInt64_Real_Get(self->integer, closure);
}

inline PyObject *
AtomicInt64Handle_Real_Set(AtomicInt64Handle *self, PyObject *value, void *closure)
{
    return AtomicInt64_Real_Set(self->integer, value, closure);
}
