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
            PyUnicode_FromFormat("%R > 9223372036854775807 == (2 ** 63) - 1", py_integer)
        );  // todo: docs link
        return 0;
    }

    return 1;
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


PyObject *
AtomicInt_Get(AtomicInt *self)
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
