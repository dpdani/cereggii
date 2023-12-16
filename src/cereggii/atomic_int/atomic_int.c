// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_int.h"
#include "atomic_int_internal.h"


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

    PyObject *py_integer = PyLong_FromLong(integer);  // NULL on error => return NULL

    return py_integer;
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
