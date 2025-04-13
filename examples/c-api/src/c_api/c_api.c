#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "cereggii/atomics.h"

typedef struct {
    PyObject_HEAD
    Py_ssize_t ref;
} PyAtomic;

// Constructor: Atomic(ref)
static int PyAtomic_init(PyAtomic *self, PyObject *args, PyObject *kwargs) {
    Py_ssize_t initial;
    if (!PyArg_ParseTuple(args, "n", &initial)) {
        return -1;
    }
    self->ref = initial;
    return 0;
}

// Destructor
static void PyAtomic_dealloc(PyAtomic* self) {
    Py_TYPE(self)->tp_free((PyObject*)self);
}

// Atomic compare-and-exchange: Atomic.compare_exchange(expected, desired)
static PyObject* PyAtomic_compare_exchange(PyAtomic *self, PyObject *args) {
    Py_ssize_t expected;
    Py_ssize_t desired;
    
    if (!PyArg_ParseTuple(args, "nn", &expected, &desired)) {
        return NULL;
    }

    int success = CereggiiAtomic_CompareExchangeSsize(&self->ref, expected, desired);
    return PyLong_FromLong(success);
}

static PyObject* PyAtomic_get(PyAtomic *self, PyObject *args) {
    return PyLong_FromSsize_t(self->ref);
}

// Atomic methods
static PyMethodDef PyAtomic_methods[] = {
    {"compare_exchange", (PyCFunction)PyAtomic_compare_exchange, METH_VARARGS, ""},
    {"get", (PyCFunction)PyAtomic_get, METH_NOARGS, ""},
    {NULL}  // Sentinel
};

// Atomic type definition
static PyTypeObject PyAtomicType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "atomic_module.Atomic",
    .tp_basicsize = sizeof(PyAtomic),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) PyAtomic_init,
    .tp_dealloc = (destructor) PyAtomic_dealloc,
    .tp_methods = PyAtomic_methods,
};

// Module methods
static PyMethodDef AtomicMethods[] = {
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef atomicmodule = {
    PyModuleDef_HEAD_INIT,
    "atomic_module",
    "Python wrapper for atomic operations",
    -1,
    AtomicMethods
};

// Module initialization
PyMODINIT_FUNC PyInit__c_api(void) {
    PyObject *m;
    if (PyType_Ready(&PyAtomicType) < 0) return NULL;

    m = PyModule_Create(&atomicmodule);
    if (!m) return NULL;

    Py_INCREF(&PyAtomicType);
    if (PyModule_AddObject(m, "Atomic", (PyObject *) &PyAtomicType) < 0) {
        Py_DECREF(&PyAtomicType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
