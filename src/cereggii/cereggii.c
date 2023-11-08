// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_ref.h"
#include "atomic_dict.h"


static PyMethodDef AtomicRef_methods[] = {
    {"get",             (PyCFunction) atomic_ref_get_ref,         METH_NOARGS,  NULL},
    {"set",             (PyCFunction) atomic_ref_set_ref,         METH_O,       NULL},
    {"compare_and_set", (PyCFunction) atomic_ref_compare_and_set, METH_VARARGS, NULL},
    {"get_and_set",     (PyCFunction) atomic_ref_get_and_set,     METH_O,       NULL},
    {NULL}
};

PyTypeObject AtomicRef_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicRef",
    .tp_doc = PyDoc_STR("An object reference that may be updated atomically."),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_basicsize = sizeof(AtomicRef),
    .tp_itemsize = 0,
    .tp_new = AtomicRef_new,
    .tp_init = (initproc) AtomicRef_init,
    .tp_dealloc = (destructor) AtomicRef_dealloc,
    .tp_traverse = (traverseproc) AtomicRef_traverse,
    .tp_methods = AtomicRef_methods,
};


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
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = AtomicDict_new,
    .tp_dealloc = (destructor) AtomicDict_dealloc,
    .tp_traverse = (traverseproc) AtomicDict_traverse,
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
    if (PyType_Ready(&AtomicRef_Type) < 0)
        return NULL;

    m = PyModule_Create(&cereggii_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&AtomicDict_Type);
    if (PyModule_AddObject(m, "AtomicDict", (PyObject *) &AtomicDict_Type) < 0) {
        Py_DECREF(&AtomicDict_Type);
        goto fail;
    }

    Py_INCREF(&AtomicRef_Type);
    if (PyModule_AddObject(m, "AtomicRef", (PyObject *) &AtomicRef_Type) < 0) {
        Py_DECREF(&AtomicRef_Type);
        goto fail;
    }

    return m;
    fail:
    Py_DECREF(m);
    return NULL;
}
