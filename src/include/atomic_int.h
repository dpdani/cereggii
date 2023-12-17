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
} AtomicInt;

typedef struct atomic_int_handle {
    PyObject_HEAD

    AtomicInt *integer;
} AtomicIntHandle;


/// basic methods

int64_t AtomicInt_Get(AtomicInt *self);

PyObject *AtomicInt_Get_callable(AtomicInt *self);

void AtomicInt_Set(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_Set_callable(AtomicInt *self, PyObject *updated);

int AtomicInt_CompareAndSet(AtomicInt *self, int64_t expected, int64_t updated);

PyObject *AtomicInt_CompareAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);

int64_t AtomicInt_GetAndSet(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_GetAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);


/// handle

PyObject *AtomicInt_GetHandle(AtomicInt *self);


/// java-esque methods
// see https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicInteger.html

int64_t AtomicInt_IncrementAndGet(AtomicInt *self, int64_t other, int *overflow);

PyObject *AtomicInt_IncrementAndGet_callable(AtomicInt *self, PyObject *py_other);

int64_t AtomicInt_GetAndIncrement(AtomicInt *self, int64_t other, int *overflow);

PyObject *AtomicInt_GetAndIncrement_callable(AtomicInt *self, PyObject *py_other);


int64_t AtomicInt_DecrementAndGet(AtomicInt *self, int64_t other, int *overflow);

PyObject *AtomicInt_DecrementAndGet_callable(AtomicInt *self, PyObject *other);

int64_t AtomicInt_GetAndDecrement(AtomicInt *self, int64_t other, int *overflow);

PyObject *AtomicInt_GetAndDecrement_callable(AtomicInt *self, PyObject *other);


int64_t AtomicInt_GetAndUpdate(AtomicInt *self, PyObject *callable, int *error);

PyObject *AtomicInt_GetAndUpdate_callable(AtomicInt *self, PyObject *callable);

int64_t AtomicInt_UpdateAndGet(AtomicInt *self, PyObject *callable, int *error);

PyObject *AtomicInt_UpdateAndGet_callable(AtomicInt *self, PyObject *callable);


/// number methods

Py_hash_t AtomicInt_Hash(AtomicInt *self);

PyObject *AtomicInt_Add(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Subtract(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Multiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Remainder(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Divmod(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Power(AtomicInt *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt_Negative(AtomicInt *self);

PyObject *AtomicInt_Positive(AtomicInt *self);

PyObject *AtomicInt_Absolute(AtomicInt *self);

int AtomicInt_Bool(AtomicInt *self);

PyObject *AtomicInt_Invert(AtomicInt *self);

PyObject *AtomicInt_Lshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Rshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_And(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Xor(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Or(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Int(AtomicInt *self);

PyObject *AtomicInt_Float(AtomicInt *self);


PyObject *AtomicInt_InplaceAdd(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceSubtract(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceRemainder(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplacePower(AtomicInt *self, PyObject *other, PyObject *mod);

PyObject *AtomicInt_InplaceLshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceRshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceAnd(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceXor(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceOr(AtomicInt *self, PyObject *other);


PyObject *AtomicInt_FloorDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_TrueDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceFloorDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceTrueDivide(AtomicInt *self, PyObject *other);


PyObject *AtomicInt_Index(AtomicInt *self);


PyObject *AtomicInt_MatrixMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceMatrixMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_RichCompare(AtomicInt *self, PyObject *other, int op);


/// int-specific methods
// these are explicitly not supported

PyObject *AtomicInt_AsIntegerRatio(AtomicInt *self);

PyObject *AtomicInt_BitLength(AtomicInt *self);

PyObject *AtomicInt_Conjugate(AtomicInt *self);

PyObject *AtomicInt_FromBytes(AtomicInt *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt_ToBytes(AtomicInt *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicInt_Denominator_Get(AtomicInt *self, void *closure);

PyObject *AtomicInt_Denominator_Set(AtomicInt *self, PyObject *value, void *closure);

PyObject *AtomicInt_Numerator_Get(AtomicInt *self, void *closure);

PyObject *AtomicInt_Numerator_Set(AtomicInt *self, PyObject *value, void *closure);

PyObject *AtomicInt_Imag_Get(AtomicInt *self, void *closure);

PyObject *AtomicInt_Imag_Set(AtomicInt *self, PyObject *value, void *closure);

PyObject *AtomicInt_Real_Get(AtomicInt *self, void *closure);

PyObject *AtomicInt_Real_Set(AtomicInt *self, PyObject *value, void *closure);


/// handle

__attribute__((unused)) int64_t AtomicIntHandle_Get(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Get_callable(AtomicIntHandle *self);

__attribute__((unused)) void AtomicIntHandle_Set(AtomicIntHandle *self, int64_t updated);

PyObject *AtomicIntHandle_Set_callable(AtomicIntHandle *self, PyObject *updated);

__attribute__((unused)) int AtomicIntHandle_CompareAndSet(AtomicIntHandle *self, int64_t expected, int64_t updated);

PyObject *AtomicIntHandle_CompareAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

__attribute__((unused)) int64_t AtomicIntHandle_GetAndSet(AtomicIntHandle *self, int64_t updated);

PyObject *AtomicIntHandle_GetAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_GetHandle(AtomicIntHandle *self);

__attribute__((unused)) int64_t AtomicIntHandle_IncrementAndGet(AtomicIntHandle *self, int64_t other, int *overflow);

PyObject *AtomicIntHandle_IncrementAndGet_callable(AtomicIntHandle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicIntHandle_GetAndIncrement(AtomicIntHandle *self, int64_t other, int *overflow);

PyObject *AtomicIntHandle_GetAndIncrement_callable(AtomicIntHandle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicIntHandle_DecrementAndGet(AtomicIntHandle *self, int64_t other, int *overflow);

PyObject *AtomicIntHandle_DecrementAndGet_callable(AtomicIntHandle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicIntHandle_GetAndDecrement(AtomicIntHandle *self, int64_t other, int *overflow);

PyObject *AtomicIntHandle_GetAndDecrement_callable(AtomicIntHandle *self, PyObject *other);

__attribute__((unused)) int64_t AtomicIntHandle_GetAndUpdate(AtomicIntHandle *self, PyObject *callable, int *overflow);

PyObject *AtomicIntHandle_GetAndUpdate_callable(AtomicIntHandle *self, PyObject *callable);

__attribute__((unused)) int64_t AtomicIntHandle_UpdateAndGet(AtomicIntHandle *self, PyObject *callable, int *overflow);

PyObject *AtomicIntHandle_UpdateAndGet_callable(AtomicIntHandle *self, PyObject *callable);

Py_hash_t AtomicIntHandle_Hash(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Add(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Subtract(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Multiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Remainder(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Divmod(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Power(AtomicIntHandle *self, PyObject *other, PyObject *mod);

PyObject *AtomicIntHandle_Negative(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Positive(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Absolute(AtomicIntHandle *self);

int AtomicIntHandle_Bool(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Invert(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Lshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Rshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_And(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Xor(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Or(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Int(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Float(AtomicIntHandle *self);

PyObject *AtomicIntHandle_InplaceAdd(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceSubtract(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceRemainder(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplacePower(AtomicIntHandle *self, PyObject *other, PyObject *mod);

PyObject *AtomicIntHandle_InplaceLshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceRshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceAnd(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceXor(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceOr(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_FloorDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_TrueDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceFloorDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceTrueDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Index(AtomicIntHandle *self);

PyObject *AtomicIntHandle_MatrixMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceMatrixMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_RichCompare(AtomicIntHandle *self, PyObject *other, int op);

PyObject *AtomicIntHandle_AsIntegerRatio(AtomicIntHandle *self);

PyObject *AtomicIntHandle_BitLength(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Conjugate(AtomicIntHandle *self);

PyObject *AtomicIntHandle_FromBytes(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_ToBytes(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_Denominator_Get(AtomicIntHandle *self, void *closure);

PyObject *AtomicIntHandle_Denominator_Set(AtomicIntHandle *self, PyObject *value, void *closure);

PyObject *AtomicIntHandle_Numerator_Get(AtomicIntHandle *self, void *closure);

PyObject *AtomicIntHandle_Numerator_Set(AtomicIntHandle *self, PyObject *value, void *closure);

PyObject *AtomicIntHandle_Imag_Get(AtomicIntHandle *self, void *closure);

PyObject *AtomicIntHandle_Imag_Set(AtomicIntHandle *self, PyObject *value, void *closure);

PyObject *AtomicIntHandle_Real_Get(AtomicIntHandle *self, void *closure);

PyObject *AtomicIntHandle_Real_Set(AtomicIntHandle *self, PyObject *value, void *closure);


/// type methods

PyObject *AtomicInt_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

int AtomicInt_init(AtomicInt *self, PyObject *args, PyObject *kwargs);

int AtomicIntHandle_init(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

void AtomicInt_dealloc(AtomicInt *self);

void AtomicIntHandle_dealloc(AtomicIntHandle *self);

extern PyTypeObject AtomicInt_Type;
extern PyTypeObject AtomicIntHandle_Type;


#endif // CEREGGII_ATOMIC_INT_H
