// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_H
#define CEREGGII_ATOMIC_INT_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>


typedef struct atomic_int {
    PyObject_HEAD

    int64_t integer;
} AtomicInt64;

typedef struct atomic_int_handle {
    PyObject_HEAD

    AtomicInt64 *integer;
} AtomicInt64Handle;


/// basic methods

int64_t AtomicInt64_Get(AtomicInt64 *self);

PyObject *AtomicInt64_Get_callable(AtomicInt64 *self);

void AtomicInt64_Set(AtomicInt64 *self, int64_t desired);

PyObject *AtomicInt64_Set_callable(AtomicInt64 *self, PyObject *updated);

int AtomicInt64_CompareAndSet(AtomicInt64 *self, int64_t expected, int64_t desired);

PyObject *AtomicInt64_CompareAndSet_callable(AtomicInt64 *self, PyObject *args, PyObject *kwargs);

int64_t AtomicInt64_GetAndSet(AtomicInt64 *self, int64_t desired);

PyObject *AtomicInt64_GetAndSet_callable(AtomicInt64 *self, PyObject *args, PyObject *kwargs);


/// handle

PyObject *AtomicInt64_GetHandle(AtomicInt64 *self);


/// java-esque methods
// see https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicInteger.html

int64_t AtomicInt64_IncrementAndGet(AtomicInt64 *self, int64_t other, int *overflow);

PyObject *AtomicInt64_IncrementAndGet_callable(AtomicInt64 *self, PyObject *py_other);

int64_t AtomicInt64_GetAndIncrement(AtomicInt64 *self, int64_t other, int *overflow);

PyObject *AtomicInt64_GetAndIncrement_callable(AtomicInt64 *self, PyObject *py_other);


int64_t AtomicInt64_DecrementAndGet(AtomicInt64 *self, int64_t other, int *overflow);

PyObject *AtomicInt64_DecrementAndGet_callable(AtomicInt64 *self, PyObject *other);

int64_t AtomicInt64_GetAndDecrement(AtomicInt64 *self, int64_t other, int *overflow);

PyObject *AtomicInt64_GetAndDecrement_callable(AtomicInt64 *self, PyObject *other);


int64_t AtomicInt64_GetAndUpdate(AtomicInt64 *self, PyObject *callable, int *error);

PyObject *AtomicInt64_GetAndUpdate_callable(AtomicInt64 *self, PyObject *callable);

int64_t AtomicInt64_UpdateAndGet(AtomicInt64 *self, PyObject *callable, int *error);

PyObject *AtomicInt64_UpdateAndGet_callable(AtomicInt64 *self, PyObject *callable);


/// number methods

Py_hash_t AtomicInt64_Hash(AtomicInt64 *self);

PyObject *AtomicInt64_Add(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Subtract(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Multiply(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Remainder(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Divmod(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Power(AtomicInt64 *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt64_Negative(AtomicInt64 *self);

PyObject *AtomicInt64_Positive(AtomicInt64 *self);

PyObject *AtomicInt64_Absolute(AtomicInt64 *self);

int AtomicInt64_Bool(AtomicInt64 *self);

PyObject *AtomicInt64_Invert(AtomicInt64 *self);

PyObject *AtomicInt64_Lshift(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Rshift(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_And(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Xor(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Or(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_Int(AtomicInt64 *self);

PyObject *AtomicInt64_Float(AtomicInt64 *self);


PyObject *AtomicInt64_InplaceAdd(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceSubtract(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceMultiply(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceRemainder(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplacePower(AtomicInt64 *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt64_InplaceLshift(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceRshift(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceAnd(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceXor(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceOr(AtomicInt64 *self, PyObject *other);


PyObject *AtomicInt64_FloorDivide(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_TrueDivide(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceFloorDivide(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceTrueDivide(AtomicInt64 *self, PyObject *other);


PyObject *AtomicInt64_Index(AtomicInt64 *self);


PyObject *AtomicInt64_MatrixMultiply(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_InplaceMatrixMultiply(AtomicInt64 *self, PyObject *other);

PyObject *AtomicInt64_RichCompare(AtomicInt64 *self, PyObject *other, int op);


/// int-specific methods
// these are explicitly not supported

PyObject *AtomicInt64_AsIntegerRatio(AtomicInt64 *self);

PyObject *AtomicInt64_BitLength(AtomicInt64 *self);

PyObject *AtomicInt64_Conjugate(AtomicInt64 *self);

PyObject *AtomicInt64_FromBytes(AtomicInt64 *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt64_ToBytes(AtomicInt64 *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt64_Denominator_Get(AtomicInt64 *self, void *closure);

PyObject *AtomicInt64_Denominator_Set(AtomicInt64 *self, PyObject *value, void *closure);

PyObject *AtomicInt64_Numerator_Get(AtomicInt64 *self, void *closure);

PyObject *AtomicInt64_Numerator_Set(AtomicInt64 *self, PyObject *value, void *closure);

PyObject *AtomicInt64_Imag_Get(AtomicInt64 *self, void *closure);

PyObject *AtomicInt64_Imag_Set(AtomicInt64 *self, PyObject *value, void *closure);

PyObject *AtomicInt64_Real_Get(AtomicInt64 *self, void *closure);

PyObject *AtomicInt64_Real_Set(AtomicInt64 *self, PyObject *value, void *closure);


/// handle

__attribute__((unused)) int64_t AtomicInt64Handle_Get(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Get_callable(AtomicInt64Handle *self);

__attribute__((unused)) void AtomicInt64Handle_Set(AtomicInt64Handle *self, int64_t updated);

PyObject *AtomicInt64Handle_Set_callable(AtomicInt64Handle *self, PyObject *updated);

__attribute__((unused)) int AtomicInt64Handle_CompareAndSet(AtomicInt64Handle *self, int64_t expected, int64_t updated);

PyObject *AtomicInt64Handle_CompareAndSet_callable(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs);

__attribute__((unused)) int64_t AtomicInt64Handle_GetAndSet(AtomicInt64Handle *self, int64_t updated);

PyObject *AtomicInt64Handle_GetAndSet_callable(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt64Handle_GetHandle(AtomicInt64Handle *self);

__attribute__((unused)) int64_t AtomicInt64Handle_IncrementAndGet(AtomicInt64Handle *self, int64_t other, int *overflow);

PyObject *AtomicInt64Handle_IncrementAndGet_callable(AtomicInt64Handle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicInt64Handle_GetAndIncrement(AtomicInt64Handle *self, int64_t other, int *overflow);

PyObject *AtomicInt64Handle_GetAndIncrement_callable(AtomicInt64Handle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicInt64Handle_DecrementAndGet(AtomicInt64Handle *self, int64_t other, int *overflow);

PyObject *AtomicInt64Handle_DecrementAndGet_callable(AtomicInt64Handle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicInt64Handle_GetAndDecrement(AtomicInt64Handle *self, int64_t other, int *overflow);

PyObject *AtomicInt64Handle_GetAndDecrement_callable(AtomicInt64Handle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicInt64Handle_GetAndUpdate(AtomicInt64Handle *self, PyObject *callable, int *overflow);

PyObject *AtomicInt64Handle_GetAndUpdate_callable(AtomicInt64Handle *self, PyObject *callable);

__attribute__((unused)) int64_t AtomicInt64Handle_UpdateAndGet(AtomicInt64Handle *self, PyObject *callable, int *overflow);

PyObject *AtomicInt64Handle_UpdateAndGet_callable(AtomicInt64Handle *self, PyObject *callable);

Py_hash_t AtomicInt64Handle_Hash(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Add(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Subtract(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Multiply(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Remainder(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Divmod(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Power(AtomicInt64Handle *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt64Handle_Negative(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Positive(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Absolute(AtomicInt64Handle *self);

int AtomicInt64Handle_Bool(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Invert(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Lshift(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Rshift(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_And(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Xor(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Or(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Int(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Float(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_InplaceAdd(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceSubtract(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceMultiply(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceRemainder(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplacePower(AtomicInt64Handle *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt64Handle_InplaceLshift(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceRshift(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceAnd(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceXor(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceOr(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_FloorDivide(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_TrueDivide(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceFloorDivide(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceTrueDivide(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_Index(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_MatrixMultiply(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_InplaceMatrixMultiply(AtomicInt64Handle *self, PyObject *other);

PyObject *AtomicInt64Handle_RichCompare(AtomicInt64Handle *self, PyObject *other, int op);

PyObject *AtomicInt64Handle_AsIntegerRatio(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_BitLength(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_Conjugate(AtomicInt64Handle *self);

PyObject *AtomicInt64Handle_FromBytes(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt64Handle_ToBytes(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt64Handle_Denominator_Get(AtomicInt64Handle *self, void *closure);

PyObject *AtomicInt64Handle_Denominator_Set(AtomicInt64Handle *self, PyObject *value, void *closure);

PyObject *AtomicInt64Handle_Numerator_Get(AtomicInt64Handle *self, void *closure);

PyObject *AtomicInt64Handle_Numerator_Set(AtomicInt64Handle *self, PyObject *value, void *closure);

PyObject *AtomicInt64Handle_Imag_Get(AtomicInt64Handle *self, void *closure);

PyObject *AtomicInt64Handle_Imag_Set(AtomicInt64Handle *self, PyObject *value, void *closure);

PyObject *AtomicInt64Handle_Real_Get(AtomicInt64Handle *self, void *closure);

PyObject *AtomicInt64Handle_Real_Set(AtomicInt64Handle *self, PyObject *value, void *closure);


/// type methods

PyObject *AtomicInt64_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

int AtomicInt64_init(AtomicInt64 *self, PyObject *args, PyObject *kwargs);

int AtomicInt64Handle_init(AtomicInt64Handle *self, PyObject *args, PyObject *kwargs);

void AtomicInt64_dealloc(AtomicInt64 *self);

void AtomicInt64Handle_dealloc(AtomicInt64Handle *self);

extern PyTypeObject AtomicInt64_Type;
extern PyTypeObject AtomicInt64Handle_Type;


#endif // CEREGGII_ATOMIC_INT_H
