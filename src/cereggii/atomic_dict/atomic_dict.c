#define PY_SSIZE_T_CLEAN

#include <Python.h>


#define CACHE_LINE_SIZE 64 // in bytes


typedef struct {
    char flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} AtomicDictEntry;


typedef struct {
    PyObject *generation;
//    PyObject *iteration;

    AtomicDictEntry entries[64];
} AtomicDictBlock;

typedef struct {
    PyObject_HEAD

    unsigned int log_size;

    unsigned long *index;

    unsigned long node_mask;
    unsigned long index_mask;
    unsigned long distance_mask;
    unsigned long tag_mask;
} AtomicDictMeta;

typedef struct {
    PyObject_HEAD

    AtomicDictMeta metadata;
    AtomicDictBlock *blocks;
} AtomicDict;

static PyTypeObject AtomicDictType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicDict",
    .tp_doc = PyDoc_STR("A thread-safe dictionary (hashmap), that's almost-lock-free™."),
    .tp_basicsize = sizeof(AtomicDict),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
};

PyObject *
atomic_dict_lookup(AtomicDict *dk, PyObject *key)
{
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
}

PyObject *
atomic_dict_lookup_call(PyObject *self, PyObject *args)
{
    PyObject *dk, *key;

    if (!PyArg_ParseTuple(args, "OO", &dk, &key))
        return NULL;

    return atomic_dict_lookup(dk, key);
}


static PyMethodDef AtomicDictMethods[] = {
    {"lookup", atomic_dict_lookup_call, METH_VARARGS, NULL},
    {NULL, NULL, 0,                                   NULL}
};

static PyModuleDef atomic_dict_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_atomic_dict",
    .m_doc = NULL,
    .m_size = -1,
};

__attribute__((unused)) PyMODINIT_FUNC
PyInit__atomic_dict(void)
{
    PyObject *m;

    if (PyType_Ready(&AtomicDictType) < 0)
        return NULL;

    m = PyModule_Create(&atomic_dict_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&AtomicDictType);
    if (PyModule_AddObject(m, "AtomicDict", (PyObject *) &AtomicDictType) < 0) {
        Py_DECREF(&AtomicDictType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
