#define PY_SSIZE_T_CLEAN

#include <Python.h>


#define CACHE_LINE_SIZE 64 // in bytes


typedef struct CereggiiDictEntry {
    char flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} CereggiiDictEntry;


typedef struct CereggiiDictBlock {
    PyObject *generation;
//    PyObject *iteration;

    CereggiiDictEntry entries[64];
} CereggiiDictBlock;

typedef struct CereggiiDictMeta {
    PyObject_HEAD

    unsigned int log_size;

    unsigned long *index;

    unsigned long node_mask;
    unsigned long index_mask;
    unsigned long distance_mask;
    unsigned long tag_mask;
} CereggiiDictMeta;

typedef struct CereggiiDict {
    PyObject_HEAD

    CereggiiDictMeta metadata;
    CereggiiDictBlock *blocks;
} CereggiiDict;

PyObject*
atomic_dict_lookup(CereggiiDict *dict, PyObject *key)
{
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
}

PyObject*
atomic_dict_lookup_call(PyObject *self, PyObject *args)
{
    PyObject *dict, *key;

    if (!PyArg_ParseTuple(args, "OO", &dict, &key))
        return NULL;

    return atomic_dict_lookup(dict, key);
}


static PyMethodDef AtomicDictMethods[] = {
        {"lookup", atomic_dict_lookup_call, METH_VARARGS, NULL},
        {NULL, NULL, 0, NULL}
};

static struct PyModuleDef atomic_dict_module = {
        PyModuleDef_HEAD_INIT,
        "_atomic_dict",
        NULL,
        -1,
        AtomicDictMethods,
};

__attribute__((unused)) PyMODINIT_FUNC
PyInit__atomic_dict(void) {
    PyObject *m;

    m = PyModule_Create(&atomic_dict_module);
    if (m == NULL)
        return NULL;

    return m;
}
