// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_int.h"
#include "atomic_int_internal.h"

#include "pyhash.h"


inline int
AtomicInt_ConvertToCLongOrSetException(PyObject *py_integer /* borrowed */, int64_t *integer)
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
AtomicInt_AddOrSetOverflow(int64_t current, int64_t to_add, int64_t *result)
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
AtomicInt_SubOrSetOverflow(int64_t current, int64_t to_sub, int64_t *result)
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
AtomicInt_MulOrSetOverflow(int64_t current, int64_t to_mul, int64_t *result)
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
AtomicInt_DivOrSetException(int64_t current, int64_t to_div, int64_t *result)
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


PyObject *
AtomicInt_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    AtomicInt *self;
    self = (AtomicInt *) type->tp_alloc(type, 0);

    return (PyObject *) self;
}

int
AtomicInt_init(AtomicInt *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_integer = NULL;

    char *kw_list[] = {"initial_value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kw_list, &py_integer))
        goto fail;

    if (py_integer != NULL) {
        if (!AtomicInt_ConvertToCLongOrSetException(py_integer, &self->integer))
            goto fail;
    } else {
        self->integer = 0;
    }

    return 0;

    fail:
    Py_XDECREF(py_integer);
    return -1;
}

void
AtomicInt_dealloc(AtomicInt *self)
{
    Py_TYPE(self)->tp_free((PyObject *) self);
}


inline int64_t
AtomicInt_Get(AtomicInt *self)
{
    return self->integer;
}

PyObject *
AtomicInt_Get_callable(AtomicInt *self)
{
    int64_t integer = self->integer;

    return PyLong_FromLong(integer);  // NULL on error => return NULL
}

void
AtomicInt_Set(AtomicInt *self, int64_t updated)
{
    int64_t current = self->integer;

    while (!_Py_atomic_compare_exchange_int64(&self->integer, current, updated)) {
        current = self->integer;
    }
}

PyObject *
AtomicInt_Set_callable(AtomicInt *self, PyObject *py_integer)
{
    assert(py_integer != NULL);

    int64_t updated;

    if (!AtomicInt_ConvertToCLongOrSetException(py_integer, &updated))
        goto fail;

    AtomicInt_Set(self, updated);

    Py_RETURN_NONE;
    fail:
    return NULL;
}

inline int
AtomicInt_CompareAndSet(AtomicInt *self, int64_t expected, int64_t updated)
{
    return _Py_atomic_compare_exchange_int64(&self->integer, expected, updated);
}

PyObject *
AtomicInt_CompareAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_expected;
    PyObject *py_updated;
    int64_t expected;
    int64_t updated;

    char *kw_list[] = {"expected", "updated", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kw_list, &py_expected, &py_updated)) {
        return NULL;
    }

    if (!AtomicInt_ConvertToCLongOrSetException(py_expected, &expected)
        || !AtomicInt_ConvertToCLongOrSetException(py_updated, &updated))
        return NULL;

    if (AtomicInt_CompareAndSet(self, expected, updated)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

inline int64_t
AtomicInt_GetAndSet(AtomicInt *self, int64_t updated)
{
    return _Py_atomic_exchange_int64(&self->integer, updated);
}

PyObject *
AtomicInt_GetAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs)
{
    PyObject *py_integer = NULL;
    int64_t updated;

    char *kw_list[] = {"updated", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &py_integer)) {
        return NULL;
    }

    if (!AtomicInt_ConvertToCLongOrSetException(py_integer, &updated))
        return NULL;

    int64_t previous = AtomicInt_GetAndSet(self, updated);

    return PyLong_FromLong(previous);  // may fail and return NULL
}

int64_t
AtomicInt_IncrementAndGet(AtomicInt *self, int64_t other, int *overflow)
{
    int64_t current, updated;

    do {
        current = AtomicInt_Get(self);

        if ((*overflow = AtomicInt_AddOrSetOverflow(current, other, &updated)))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return updated;
    fail:
    return -1;
}

PyObject *
AtomicInt_IncrementAndGet_callable(AtomicInt *self, PyObject *args)
{
    int overflow;
    int64_t other, updated;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    updated = AtomicInt_IncrementAndGet(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}

int64_t
AtomicInt_GetAndIncrement(AtomicInt *self, int64_t other, int *overflow)
{
    int64_t current, updated;

    do {
        current = AtomicInt_Get(self);

        if ((*overflow = AtomicInt_AddOrSetOverflow(current, other, &updated)))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return current;
    fail:
    return -1;
}

PyObject *
AtomicInt_GetAndIncrement_callable(AtomicInt *self, PyObject *args)
{
    int overflow;
    int64_t other, updated;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    updated = AtomicInt_GetAndIncrement(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}

int64_t
AtomicInt_DecrementAndGet(AtomicInt *self, int64_t other, int *overflow)
{
    int64_t current, updated;

    do {
        current = AtomicInt_Get(self);

        if ((*overflow = AtomicInt_SubOrSetOverflow(current, other, &updated)))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return updated;
    fail:
    return -1;
}

PyObject *
AtomicInt_DecrementAndGet_callable(AtomicInt *self, PyObject *args)
{
    int overflow;
    int64_t other, updated;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    updated = AtomicInt_DecrementAndGet(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}

int64_t
AtomicInt_GetAndDecrement(AtomicInt *self, int64_t other, int *overflow)
{
    int64_t current, updated;

    do {
        current = AtomicInt_Get(self);

        if ((*overflow = AtomicInt_SubOrSetOverflow(current, other, &updated)))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return current;
    fail:
    return -1;
}

PyObject *
AtomicInt_GetAndDecrement_callable(AtomicInt *self, PyObject *args)
{
    int overflow;
    int64_t other, updated;
    PyObject *py_other = NULL;

    if (!PyArg_ParseTuple(args, "|O", &py_other))
        goto fail;

    if (py_other == NULL || py_other == Py_None) {
        other = 1;
    } else {
        if (!AtomicInt_ConvertToCLongOrSetException(py_other, &other))
            goto fail;
    }

    updated = AtomicInt_GetAndDecrement(self, other, &overflow);

    if (overflow)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}

int64_t
AtomicInt_GetAndUpdate(AtomicInt *self, PyObject *callable, int *error)
{
    int64_t current, updated;
    PyObject *py_current = NULL, *py_updated = NULL;
    *error = 0;

    do {
        current = AtomicInt_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_updated = PyObject_CallOneArg(callable, py_current);

        if (!AtomicInt_ConvertToCLongOrSetException(py_updated, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return current;
    fail:
    *error = 1;
    return -1;
}

PyObject *
AtomicInt_GetAndUpdate_callable(AtomicInt *self, PyObject *callable)
{
    int error;
    int64_t updated;

    updated = AtomicInt_GetAndUpdate(self, callable, &error);

    if (error)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}

int64_t
AtomicInt_UpdateAndGet(AtomicInt *self, PyObject *callable, int *error)
{
    int64_t current, updated;
    PyObject *py_current = NULL, *py_updated = NULL;
    *error = 0;

    do {
        current = AtomicInt_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_updated = PyObject_CallOneArg(callable, py_current);

        if (!AtomicInt_ConvertToCLongOrSetException(py_updated, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    return updated;
    fail:
    *error = 1;
    return -1;
}

PyObject *
AtomicInt_UpdateAndGet_callable(AtomicInt *self, PyObject *callable)
{
    int error;
    int64_t updated;

    updated = AtomicInt_UpdateAndGet(self, callable, &error);

    if (error)
        goto fail;

    return PyLong_FromLong(updated);
    fail:
    return NULL;
}


PyObject *
AtomicInt_GetHandle(AtomicInt *self)
{
    PyObject *handle = NULL;

    handle = AtomicIntHandle_new(&AtomicIntHandle_Type, NULL, NULL);

    if (handle == NULL)
        goto fail;

    PyObject *args = Py_BuildValue("(O)", self);
    if (AtomicIntHandle_init((AtomicIntHandle *) handle, args, NULL) < 0)
        goto fail;

    return handle;

    fail:
    return NULL;
}

Py_hash_t
AtomicInt_Hash(AtomicInt *self)
{
    return _Py_HashPointer(self);
}

inline PyObject *
AtomicInt_Add(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Add(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Subtract(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Subtract(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Multiply(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Multiply(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Remainder(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Remainder(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Divmod(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Divmod(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Power(AtomicInt *self, PyObject *other, PyObject *mod)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Power(current, other, mod);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Negative(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Negative(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Positive(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Positive(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Absolute(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Absolute(current);
    fail:
    return NULL;
}

inline int
AtomicInt_Bool(AtomicInt *self)
{
    int64_t current = AtomicInt_Get(self);

    if (current)
        return 1;
    else
        return 0;
}

inline PyObject *
AtomicInt_Invert(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Invert(current);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Lshift(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Lshift(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Rshift(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Rshift(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_And(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_And(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Xor(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Xor(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Or(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Or(current, other);
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_Int(AtomicInt *self)
{
    return AtomicInt_Get_callable(self);
}

inline PyObject *
AtomicInt_Float(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Float(current);
    fail:
    return NULL;
}

PyObject *
AtomicInt_FloorDivide(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_FloorDivide(current, other);
    fail:
    return NULL;
}

PyObject *
AtomicInt_TrueDivide(AtomicInt *self, PyObject *other)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_TrueDivide(current, other);
    fail:
    return NULL;
}

PyObject *
AtomicInt_Index(AtomicInt *self)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyNumber_Index(current);
    fail:
    return NULL;
}


PyObject *
AtomicInt_InplaceAdd_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);

        if (AtomicInt_AddOrSetOverflow(current, amount, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceAdd(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceAdd_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceSubtract_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);

        if (AtomicInt_SubOrSetOverflow(current, amount, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceSubtract(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceSubtract_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceMultiply_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);

        if (AtomicInt_MulOrSetOverflow(current, amount, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceMultiply(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceMultiply_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceRemainder_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current % amount;
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceRemainder(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceRemainder_internal(self, other, 1);
}

PyObject *
AtomicInt_InplacePower_internal(AtomicInt *self, PyObject *other, PyObject *mod, int do_refcount)
{
    int64_t current, updated;
    PyObject *py_current, *py_updated;

    do {
        current = AtomicInt_Get(self);
        py_current = PyLong_FromLong(current);

        if (py_current == NULL)
            goto fail;

        py_updated = PyNumber_Power(py_current, other, mod);
        if (py_updated == NULL)
            goto fail;

        if (!AtomicInt_ConvertToCLongOrSetException(py_updated, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    Py_DECREF(py_current);
    Py_DECREF(py_updated);
    return (PyObject *) self;
    fail:
    Py_XDECREF(py_current);
    Py_XDECREF(py_updated);
    return NULL;
}

inline PyObject *
AtomicInt_InplacePower(AtomicInt *self, PyObject *other, PyObject *mod)
{
    return AtomicInt_InplacePower_internal(self, other, mod, 1);
}

PyObject *
AtomicInt_InplaceLshift_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current << amount;  // todo: raise overflow?
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceLshift(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceLshift_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceRshift_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current >> amount;
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceRshift(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceRshift_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceAnd_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current & amount;
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceAnd(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceAnd_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceXor_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current ^ amount;
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceXor(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceXor_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceOr_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);
        updated = current | amount;
    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceOr(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceOr_internal(self, other, 1);
}

PyObject *
AtomicInt_InplaceFloorDivide_internal(AtomicInt *self, PyObject *other, int do_refcount)
{
    int64_t amount, current, updated;

    if (!AtomicInt_ConvertToCLongOrSetException(other, &amount))
        goto fail;

    do {
        current = AtomicInt_Get(self);

        if (AtomicInt_DivOrSetException(current, amount, &updated))
            goto fail;

    } while (!AtomicInt_CompareAndSet(self, current, updated));

    if (do_refcount)
        Py_XINCREF(self);

    return (PyObject *) self;
    fail:
    return NULL;
}

inline PyObject *
AtomicInt_InplaceFloorDivide(AtomicInt *self, PyObject *other)
{
    return AtomicInt_InplaceFloorDivide_internal(self, other, 1);
}

inline PyObject *
AtomicInt_InplaceTrueDivide(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(other))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "AtomicInt cannot store float(), use AtomicInt() //= int() instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_MatrixMultiply(AtomicInt *Py_UNUSED(self), PyObject *other)
{
    // just raise an exception; because it's supposed to be unsupported:
    // see https://peps.python.org/pep-0465/#non-definitions-for-built-in-types
    // bonus: raise the same exception as `int(...) @ other`
    return PyNumber_MatrixMultiply(PyLong_FromLong(0), other);
}

inline PyObject *
AtomicInt_InplaceMatrixMultiply(AtomicInt *Py_UNUSED(self), PyObject *other)
{
    return PyNumber_InPlaceMatrixMultiply(PyLong_FromLong(0), other);
}

inline PyObject *
AtomicInt_RichCompare(AtomicInt *self, PyObject *other, int op)
{
    PyObject *current = NULL;

    current = AtomicInt_Get_callable(self);

    if (current == NULL)
        goto fail;

    return PyObject_RichCompare(current, other, op);

    fail:
    return NULL;
}

inline PyObject *
AtomicInt_AsIntegerRatio(AtomicInt *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().as_integer_ratio instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_BitLength(AtomicInt *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().bit_length instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Conjugate(AtomicInt *Py_UNUSED(self))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().conjugate instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_FromBytes(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use int.from_bytes instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_ToBytes(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().to_bytes instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Denominator_Get(AtomicInt *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().denominator.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Denominator_Set(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().denominator.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Numerator_Get(AtomicInt *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().numerator.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Numerator_Set(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().numerator.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Imag_Get(AtomicInt *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().imag.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Imag_Set(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().imag.__set__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Real_Get(AtomicInt *Py_UNUSED(self), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().real.__get__ instead."
    );
    return NULL;
}

inline PyObject *
AtomicInt_Real_Set(AtomicInt *Py_UNUSED(self), PyObject *Py_UNUSED(value), void *Py_UNUSED(closure))
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "use AtomicInt().get().real.__set__ instead."
    );
    return NULL;
}

