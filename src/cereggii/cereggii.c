// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"


static PyMethodDef AtomicDict_methods[] = {
    {"lookup",           (PyCFunction) atomic_dict_lookup,           METH_O,       NULL},
    {"insert_or_update", (PyCFunction) atomic_dict_insert_or_update, METH_VARARGS, NULL},
    {"debug",            (PyCFunction) atomic_dict_debug,            METH_NOARGS,  NULL},
    {NULL}
};

static PyMappingMethods AtomicDict_mapping_methods = {
    .mp_length = (lenfunc) NULL, // atomic_dict_imprecise_length / atomic_dict_precise_length
    .mp_subscript = (binaryfunc) atomic_dict_lookup,
    .mp_ass_subscript = (objobjargproc) atomic_dict_insert_or_update,
};

static PyTypeObject AtomicDict_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicDict",
    .tp_doc = PyDoc_STR("A thread-safe dictionary (hashmap), that's almost-lock-free™."),
    .tp_basicsize = sizeof(AtomicDict),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = AtomicDict_new,
    .tp_init = (initproc) AtomicDict_init,
    .tp_methods = AtomicDict_methods,
    .tp_as_mapping = &AtomicDict_mapping_methods,
};


static PyModuleDef cereggii_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_cereggii",
    .m_doc = NULL,
    .m_size = -1,
};

__attribute__((unused)) PyMODINIT_FUNC
PyInit__cereggii(void)
{
    PyObject *m;

    if (PyType_Ready(&AtomicDict_Type) < 0)
        return NULL;

    m = PyModule_Create(&cereggii_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&AtomicDict_Type);
    if (PyModule_AddObject(m, "AtomicDict", (PyObject *) &AtomicDict_Type) < 0) {
        Py_DECREF(&AtomicDict_Type);
        goto fail;
    }

    return m;
    fail:
    Py_DECREF(m);
    return NULL;
}
