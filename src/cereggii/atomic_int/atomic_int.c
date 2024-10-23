// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_int.h"
#include "atomic_int_internal.h"
#include "atomic_ops.h"

#include "pyhash.h"


inline int
AtomicInt64_ConvertToCLongOrSetException(PyObject *py_integer /* borrowed */, int64_t *integer)
{
    if (py_integer == NULL)
        return 0;

    if (!PyLong_Check(py_integer)) {
        PyErr_SetObject(
            PyExc_TypeError,
            PyUnicode_FromFormat("not isinstance(%R, int)", py_integer)
        );
        return 0;
    }

    int overflow;
    *integer = PyLong_AsLongAndOverflow(py_integer, &overflow);
    if (PyErr_Occurred())
        return 0;

    if (overflow) {
        PyErr_SetObject(
            PyExc_OverflowError,
            PyUnicode_FromFormat("%R > %ld == (2 ** 63) - 1 or %R < %ld", py_integer, INT64_MAX, py_integer, INT64_MIN)
        );  // todo: docs link
        return 0;
    }

    return 1;
}

inline int
AtomicInt64_AddOrSetOverflow(int64_t current, int64_t to_add, int64_t *result)
{
    int overflowed = __builtin_saddl_overflow(current, to_add, result);

    if (overflowed) {
        PyErr_SetObject(
            PyExc_OverflowError,
            PyUnicode_FromFormat("%ld + %ld > %ld == (2 ** 63) - 1 "
                                 "or %ld + %ld < %ld", current, to_add, INT64_MAX, current, to_add, INT64_MIN)
        );
    }

    return overflowed;
}

inline int
AtomicInt64_SubOrSetOverflow(int64_t current, int64_t to_sub, int64_t *result)
{
    int overflowed = __builtin_ssubl_overflow(current, to_sub, result);

    if (overflowed) {
        PyErr_SetObject(
            PyExc_OverflowError,
            PyUnicode_FromFormat("%ld - %ld > %ld == (2 ** 63) - 1 "
                                 "or %ld - %ld < %ld", current, to_sub, INT64_MAX, current, to_sub, INT64_MIN)
        );
    }

    return overflowed;
}

inline int
AtomicInt64_MulOrSetOverflow(int64_t current, int64_t to_mul, int64_t *result)
{
    int overflowed = __builtin_smull_overflow(current, to_mul, result);

    if (overflowed) {
        PyErr_SetObject(
            PyExc_OverflowError,
            PyUnicode_FromFormat("%ld * %ld > %ld == (2 ** 63) - 1 "
                                 "or %ld * %ld < %ld", current, to_mul, INT64_MAX, current, to_mul, INT64_MIN)
        );
    }

    return overflowed;
}

inline int
AtomicInt64_DivOrSetException(int64_t current, int64_t to_div, int64_t *result)
{
    int overflowed = 0;

    if (to_div == 0) {
        PyErr_SetString(PyExc_ZeroDivisionError, "division by zero");
        return 1;
    }

#if (INT64_MIN != -INT64_MAX)  // => two's complement
    overflowed = (current == INT64_MIN && to_div == -1);
#endif

    *result = current / to_div;

    if (overflowed) {
        PyErr_SetObject(
            PyExc_OverflowError,
            PyUnicode_FromFormat("%ld / %ld > %ld == (2 ** 63) - 1 "
                                 "or %ld / %ld < %ld", current, to_div, INT64_MAX, current, to_div, INT64_MIN)
        );
    }

    return overflowed;
}

int
AtomicInt64_init(AtomicInt64 *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_integer = NULL;

    char *kw_list[] = {"initial_value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kw_list, &py_integer))
        goto fail;

    if (py_integer != NULL) {
        if (!AtomicInt64_ConvertToCLongOrSetException(py_integer, &self->integer))
            goto fail;
    } else {
        self->integer = 0;
    }

    return 0;

    fail:
    return -1;
}

void
AtomicInt64_dealloc(AtomicInt64 *self)
{
    Py_TYPE(self)->tp_free((PyObject *) self);
}


inline int64_t
AtomicInt64_Get(AtomicInt64 *self)
{
    return self->integer;
}

PyObject *
AtomicInt64_Get_callable(AtomicInt64 *self)
{
    int64_t integer = self->integer;

    return PyLong_FromLong(integer);  // NULL on error => return NULL
}

void
AtomicInt64_Set(AtomicInt64 *self, int64_t desired)
{
    int64_t current = self->integer;

    while (!CereggiiAtomic_CompareExchangeInt64(&self->integer, current, desired)) {
        current = self->integer;
    }
}

PyObject *
AtomicInt64_Set_callable(AtomicInt64 *self, PyObject *py_integer)
{
    assert(py_integer != NULL);

    int64_t desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(py_integer, &desired))
        goto fail;

    AtomicInt64_Set(self, desired);

    Py_RETURN_NONE;
    fail:
    return NULL;
}

inline int
AtomicInt64_CompareAndSet(AtomicInt64 *self, int64_t expected, int64_t desired)
{
    return CereggiiAtomic_CompareExchangeInt64(&self->integer, expected, desired);
}

PyObject *
AtomicInt64_CompareAndSet_callable(AtomicInt64 *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_expected;
    PyObject *py_desired;
    int64_t expected;
    int64_t desired;

    char *kw_list[] = {"expected", "desired", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kw_list, &py_expected, &py_desired)) {
        return NULL;
    }

    if (!AtomicInt64_ConvertToCLongOrSetException(py_expected, &expected)
        || !AtomicInt64_ConvertToCLongOrSetException(py_desired, &desired))
        return NULL;

    if (AtomicInt64_CompareAndSet(self, expected, desired)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

inline int64_t
AtomicInt64_GetAndSet(AtomicInt64 *self, int64_t desired)
{
    return CereggiiAtomic_ExchangeInt64(&self->integer, desired);
}

PyObject *
AtomicInt64_GetAndSet_callable(AtomicInt64 *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_integer = NULL;
    int64_t desired;

    char *kw_list[] = {"desired", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &py_integer)) {
        return NULL;
    }

    if (!AtomicInt64_ConvertToCLongOrSetException(py_integer, &desired))
        return NULL;

    int64_t previous = AtomicInt64_GetAndSet(self, desired);

    return PyLong_FromLong(previous);  // may fail and return NULL
}

int64_t
AtomicInt64_IncrementAndGet(AtomicInt64 *self, int64_t other, int *overflow)
{
    int64_t current, desired;

    do {
        current = AtomicInt64_Get(self);

        if ((*overflow = AtomicInt64_AddOrSetOverflow(current, other, &desired)))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return desired;
    fail:
    return -1;
}

PyObject *
AtomicInt64_IncrementAndGet_callable(AtomicInt64 *self, PyObject *args)
{
    int overflow;
    int64_t other, desired;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt64_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    desired = AtomicInt64_IncrementAndGet(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}

int64_t
AtomicInt64_GetAndIncrement(AtomicInt64 *self, int64_t other, int *overflow)
{
    int64_t current, desired;

    do {
        current = AtomicInt64_Get(self);

        if ((*overflow = AtomicInt64_AddOrSetOverflow(current, other, &desired)))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return current;
    fail:
    return -1;
}

PyObject *
AtomicInt64_GetAndIncrement_callable(AtomicInt64 *self, PyObject *args)
{
    int overflow;
    int64_t other, desired;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt64_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    desired = AtomicInt64_GetAndIncrement(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}

int64_t
AtomicInt64_DecrementAndGet(AtomicInt64 *self, int64_t other, int *overflow)
{
    int64_t current, desired;

    do {
        current = AtomicInt64_Get(self);

        if ((*overflow = AtomicInt64_SubOrSetOverflow(current, other, &desired)))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return desired;
    fail:
    return -1;
}

PyObject *
AtomicInt64_DecrementAndGet_callable(AtomicInt64 *self, PyObject *args)
{
    int overflow;
    int64_t other, desired;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt64_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    desired = AtomicInt64_DecrementAndGet(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}

int64_t
AtomicInt64_GetAndDecrement(AtomicInt64 *self, int64_t other, int *overflow)
{
    int64_t current, desired;

    do {
        current = AtomicInt64_Get(self);

        if ((*overflow = AtomicInt64_SubOrSetOverflow(current, other, &desired)))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return current;
    fail:
    return -1;
}

PyObject *
AtomicInt64_GetAndDecrement_callable(AtomicInt64 *self, PyObject *args)
{
    int overflow;
    int64_t other, desired;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt64_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    desired = AtomicInt64_GetAndDecrement(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}

int64_t
AtomicInt64_GetAndUpdate(AtomicInt64 *self, PyObject *callable, int *error)
{
    int64_t current, desired;
    PyObject *py_current = NULL, *py_desired = NULL;
    *error = 0;

    do {
        current = AtomicInt64_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_desired = PyObject_CallOneArg(callable, py_current);

        if (!AtomicInt64_ConvertToCLongOrSetException(py_desired, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return current;
    fail:
    *error = 1;
    return -1;
}

PyObject *
AtomicInt64_GetAndUpdate_callable(AtomicInt64 *self, PyObject *callable)
{
    int error;
    int64_t desired;

    desired = AtomicInt64_GetAndUpdate(self, callable, &error);

    if (error)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}

int64_t
AtomicInt64_UpdateAndGet(AtomicInt64 *self, PyObject *callable, int *error)
{
    int64_t current, desired;
    PyObject *py_current = NULL, *py_desired = NULL;
    *error = 0;

    do {
        current = AtomicInt64_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_desired = PyObject_CallOneArg(callable, py_current);

        if (!AtomicInt64_ConvertToCLongOrSetException(py_desired, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    return desired;
    fail:
    *error = 1;
    return -1;
}

PyObject *
AtomicInt64_UpdateAndGet_callable(AtomicInt64 *self, PyObject *callable)
{
    int error;
    int64_t desired;

    desired = AtomicInt64_UpdateAndGet(self, callable, &error);

    if (error)
        goto fail;

    return PyLong_FromLong(desired);
    fail:
    return NULL;
}


PyObject *
AtomicInt64_GetHandle(AtomicInt64 *self)
{
    AtomicInt64Handle *handle = NULL;

    handle = PyObject_New(AtomicInt64Handle, &AtomicInt64Handle_Type);

    if (handle == NULL)
        goto fail;

    PyObject *args = Py_BuildValue("(O)", self);
    if (AtomicInt64Handle_init((AtomicInt64Handle *) handle, args, NULL) < 0)
        goto fail;

    return (PyObject *) handle;

    fail:
    return NULL;
}

Py_hash_t
AtomicInt64_Hash(AtomicInt64 *self)
{
    return _Py_HashPointer(self); // this will be public in 3.13
}

inline PyObject *
AtomicInt64_Add(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Add(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Subtract(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Subtract(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Multiply(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Multiply(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Remainder(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Remainder(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Divmod(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Divmod(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Power(AtomicInt64 *self, PyObject *other, PyObject *mod)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Power(current, other, mod);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Negative(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Negative(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Positive(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Positive(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Absolute(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Absolute(current);
    fail:
    return NULL;
}

inline int
AtomicInt64_Bool(AtomicInt64 *self)
{
    int64_t current = AtomicInt64_Get(self);

    if (current)
        return 1;
    else
        return 0;
}

inline PyObject *
AtomicInt64_Invert(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Invert(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Lshift(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Lshift(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Rshift(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Rshift(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_And(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_And(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Xor(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Xor(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Or(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Or(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_Int(AtomicInt64 *self)
{
    return AtomicInt64_Get_callable(self);
}

inline PyObject *
AtomicInt64_Float(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Float(current);
    fail:
    return NULL;
}

PyObject *
AtomicInt64_FloorDivide(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_FloorDivide(current, other);
    fail:
    return NULL;
}

PyObject *
AtomicInt64_TrueDivide(AtomicInt64 *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_TrueDivide(current, other);
    fail:
    return NULL;
}

PyObject *
AtomicInt64_Index(AtomicInt64 *self)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Index(current);
    fail:
    return NULL;
}


PyObject *
AtomicInt64_InplaceAdd_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);

        if (AtomicInt64_AddOrSetOverflow(current, amount, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceAdd(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceAdd_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceSubtract_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);

        if (AtomicInt64_SubOrSetOverflow(current, amount, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceSubtract(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceSubtract_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceMultiply_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);

        if (AtomicInt64_MulOrSetOverflow(current, amount, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceMultiply(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceMultiply_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceRemainder_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current % amount;
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceRemainder(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceRemainder_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplacePower_internal(AtomicInt64 *self, PyObject *other, PyObject *mod, int do_refcount)
{
    int64_t current, desired;
    PyObject *py_current, *py_desired;

    do {
        current = AtomicInt64_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_desired = PyNumber_Power(py_current, other, mod);
        if (py_desired == NULL)
            goto fail;

        if (!AtomicInt64_ConvertToCLongOrSetException(py_desired, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    Py_DECREF(py_current);
    Py_DECREF(py_desired);
    return (PyObject *) self;
    fail:
    Py_XDECREF(py_current);
    Py_XDECREF(py_desired);
    return NULL;
}

inline PyObject *
AtomicInt64_InplacePower(AtomicInt64 *self, PyObject *other, PyObject *mod)
{
    return AtomicInt64_InplacePower_internal(self, other, mod, 1);
}

PyObject *
AtomicInt64_InplaceLshift_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current << amount;  // todo: raise overflow?
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceLshift(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceLshift_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceRshift_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current >> amount;
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceRshift(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceRshift_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceAnd_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current & amount;
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceAnd(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceAnd_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceXor_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current ^ amount;
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceXor(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceXor_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceOr_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);
        desired = current | amount;
    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceOr(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceOr_internal(self, other, 1);
}

PyObject *
AtomicInt64_InplaceFloorDivide_internal(AtomicInt64 *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, desired;

    if (!AtomicInt64_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt64_Get(self);

        if (AtomicInt64_DivOrSetException(current, amount, &desired))
            goto fail;

    } while (!AtomicInt64_CompareAndSet(self, current, desired));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_InplaceFloorDivide(AtomicInt64 *self, PyObject *other)
{
    return AtomicInt64_InplaceFloorDivide_internal(self, other, 1);
}

inline PyObject *
AtomicInt64_InplaceTrueDivide(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(other))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "AtomicInt64 cannot store float(), use AtomicInt64() //= int() instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_MatrixMultiply(AtomicInt64 *Py_UNUSED(self), PyObject *other)
{
    // just raise an exception; because it's supposed to be unsupported:
    // see https://peps.python.org/pep-0465/#non-definitions-for-built-in-types
    // bonus: raise the same exception as `int(...) @ other`
    return PyNumber_MatrixMultiply(PyLong_FromLong(0), other);
}

inline PyObject *
AtomicInt64_InplaceMatrixMultiply(AtomicInt64 *Py_UNUSED(self), PyObject *other)
{
    return PyNumber_InPlaceMatrixMultiply(PyLong_FromLong(0), other);
}

inline PyObject *
AtomicInt64_RichCompare(AtomicInt64 *self, PyObject *other, int op)
{
    PyObject *current = NULL;

    current = AtomicInt64_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyObject_RichCompare(current, other, op);

    fail:
    return NULL;
}

inline PyObject *
AtomicInt64_AsIntegerRatio(AtomicInt64 *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().as_integer_ratio instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_BitLength(AtomicInt64 *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().bit_length instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Conjugate(AtomicInt64 *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().conjugate instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_FromBytes(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use int.from_bytes instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_ToBytes(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().to_bytes instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Denominator_Get(AtomicInt64 *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().denominator.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Denominator_Set(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().denominator.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Numerator_Get(AtomicInt64 *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().numerator.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Numerator_Set(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().numerator.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Imag_Get(AtomicInt64 *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().imag.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Imag_Set(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().imag.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Real_Get(AtomicInt64 *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().real.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt64_Real_Set(AtomicInt64 *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt64().get().real.__set__ instead."
    );
    return NULL;
}

