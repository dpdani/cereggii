// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "constants.h"
#include "atomic_event.h"
#include "atomic_int.h"
#include "atomic_ref.h"
#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "thread_handle.h"


PyObject *
_generic_class_getitem(PyObject *cls, PyObject *Py_UNUSED(item))
{
    Py_INCREF(cls);
    return cls;
}


static PyMethodDef AtomicInt64_methods[] = {
    {"get",               (PyCFunction) AtomicInt64_Get_callable,             METH_NOARGS,  NULL},
    {"set",               (PyCFunction) AtomicInt64_Set_callable,             METH_O,       NULL},
    {"compare_and_set",   (PyCFunction) AtomicInt64_CompareAndSet_callable,   METH_VARARGS | METH_KEYWORDS, NULL},
    {"get_and_set",       (PyCFunction) AtomicInt64_GetAndSet_callable,       METH_VARARGS | METH_KEYWORDS, NULL},
    {"increment_and_get", (PyCFunction) AtomicInt64_IncrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_increment", (PyCFunction) AtomicInt64_GetAndIncrement_callable, METH_VARARGS, NULL},
    {"decrement_and_get", (PyCFunction) AtomicInt64_DecrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_decrement", (PyCFunction) AtomicInt64_GetAndDecrement_callable, METH_VARARGS, NULL},
    {"update_and_get",    (PyCFunction) AtomicInt64_UpdateAndGet_callable,    METH_O,       NULL},
    {"get_and_update",    (PyCFunction) AtomicInt64_GetAndUpdate_callable,    METH_O,       NULL},
    {"get_handle",        (PyCFunction) AtomicInt64_GetHandle,                METH_NOARGS,  NULL},
    {"as_integer_ratio",  (PyCFunction) AtomicInt64_AsIntegerRatio,           METH_NOARGS,  NULL},
    {"bit_length",        (PyCFunction) AtomicInt64_BitLength,                METH_NOARGS,  NULL},
    {"conjugate",         (PyCFunction) AtomicInt64_Conjugate,                METH_NOARGS,  NULL},
    {"from_bytes",        (PyCFunction) AtomicInt64_FromBytes,                METH_VARARGS | METH_KEYWORDS |
                                                                            METH_CLASS,                   NULL},
    {"to_bytes",          (PyCFunction) AtomicInt64_ToBytes,                  METH_NOARGS,  NULL},
    {NULL}
};

static PyGetSetDef AtomicInt64_properties[] = {
    {"denominator", (getter) AtomicInt64_Denominator_Get, (setter) AtomicInt64_Denominator_Set, NULL, NULL},
    {"numerator",   (getter) AtomicInt64_Numerator_Get,   (setter) AtomicInt64_Numerator_Set,   NULL, NULL},
    {"imag",        (getter) AtomicInt64_Imag_Get,        (setter) AtomicInt64_Imag_Set,        NULL, NULL},
    {"real",        (getter) AtomicInt64_Real_Get,        (setter) AtomicInt64_Real_Set,        NULL, NULL},
    {NULL},
};

static PyNumberMethods AtomicInt64_as_number = {
    .nb_add = (binaryfunc) AtomicInt64_Add,
    .nb_subtract = (binaryfunc) AtomicInt64_Subtract,
    .nb_multiply = (binaryfunc) AtomicInt64_Multiply,
    .nb_remainder = (binaryfunc) AtomicInt64_Remainder,
    .nb_divmod = (binaryfunc) AtomicInt64_Divmod,
    .nb_power = (ternaryfunc) AtomicInt64_Power,
    .nb_negative = (unaryfunc) AtomicInt64_Negative,
    .nb_positive = (unaryfunc) AtomicInt64_Positive,
    .nb_absolute = (unaryfunc) AtomicInt64_Absolute,
    .nb_bool = (inquiry) AtomicInt64_Bool,
    .nb_invert = (unaryfunc) AtomicInt64_Invert,
    .nb_lshift = (binaryfunc) AtomicInt64_Lshift,
    .nb_rshift = (binaryfunc) AtomicInt64_Rshift,
    .nb_and = (binaryfunc) AtomicInt64_And,
    .nb_xor = (binaryfunc) AtomicInt64_Xor,
    .nb_or = (binaryfunc) AtomicInt64_Or,
    .nb_int = (unaryfunc) AtomicInt64_Int,
    .nb_float = (unaryfunc) AtomicInt64_Float,

    .nb_inplace_add = (binaryfunc) AtomicInt64_InplaceAdd,
    .nb_inplace_subtract = (binaryfunc) AtomicInt64_InplaceSubtract,
    .nb_inplace_multiply = (binaryfunc) AtomicInt64_InplaceMultiply,
    .nb_inplace_remainder = (binaryfunc) AtomicInt64_InplaceRemainder,
    .nb_inplace_power = (ternaryfunc) AtomicInt64_InplacePower,
    .nb_inplace_lshift = (binaryfunc) AtomicInt64_InplaceLshift,
    .nb_inplace_rshift = (binaryfunc) AtomicInt64_InplaceRshift,
    .nb_inplace_and = (binaryfunc) AtomicInt64_InplaceAnd,
    .nb_inplace_xor = (binaryfunc) AtomicInt64_InplaceXor,
    .nb_inplace_or = (binaryfunc) AtomicInt64_InplaceOr,

    .nb_floor_divide = (binaryfunc) AtomicInt64_FloorDivide,
    .nb_true_divide = (binaryfunc) AtomicInt64_TrueDivide,
    .nb_inplace_floor_divide = (binaryfunc) AtomicInt64_InplaceFloorDivide,
    .nb_inplace_true_divide = (binaryfunc) AtomicInt64_InplaceTrueDivide,

    .nb_index = (unaryfunc) AtomicInt64_Index,

    .nb_matrix_multiply = (binaryfunc) AtomicInt64_MatrixMultiply,
    .nb_inplace_matrix_multiply = (binaryfunc) AtomicInt64_InplaceMatrixMultiply,
};

PyTypeObject AtomicInt64_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicInt64",
    .tp_doc = PyDoc_STR("An int that may be updated atomically."),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_basicsize = sizeof(AtomicInt64),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) AtomicInt64_init,
    .tp_dealloc = (destructor) AtomicInt64_dealloc,
    .tp_methods = AtomicInt64_methods,
    .tp_as_number = &AtomicInt64_as_number,
    .tp_richcompare = (richcmpfunc) AtomicInt64_RichCompare,
    .tp_hash = (hashfunc) AtomicInt64_Hash,
    .tp_getset = AtomicInt64_properties,
};


static PyMethodDef AtomicRef_methods[] = {
    {"get",               (PyCFunction) AtomicRef_Get,                    METH_NOARGS, NULL},
    {"set",               (PyCFunction) AtomicRef_Set,                    METH_O,      NULL},
    {"compare_and_set",   (PyCFunction) AtomicRef_CompareAndSet_callable, METH_VARARGS | METH_KEYWORDS, NULL},
    {"get_and_set",       (PyCFunction) AtomicRef_GetAndSet,              METH_O,      NULL},
    {"get_handle",        (PyCFunction) AtomicRef_GetHandle,              METH_NOARGS, NULL},
    {"__class_getitem__", (PyCFunction) _generic_class_getitem,           METH_O | METH_CLASS, NULL},
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
    .tp_traverse = (traverseproc) AtomicRef_traverse,
    .tp_clear = (inquiry) AtomicRef_clear,
    .tp_dealloc = (destructor) AtomicRef_dealloc,
    .tp_methods = AtomicRef_methods,
};


static PyMethodDef AtomicDict_methods[] = {
    {"_debug",            (PyCFunction) AtomicDict_Debug,                   METH_NOARGS, NULL},
    {"_rehash",           (PyCFunction) AtomicDict_ReHash,                  METH_O,      NULL},
    {"get",               (PyCFunction) AtomicDict_GetItemOrDefaultVarargs, METH_VARARGS | METH_KEYWORDS, NULL},
    {"len_bounds",        (PyCFunction) AtomicDict_LenBounds,               METH_NOARGS, NULL},
    {"approx_len",        (PyCFunction) AtomicDict_ApproxLen,               METH_NOARGS, NULL},
    {"fast_iter",         (PyCFunction) AtomicDict_FastIter,                METH_VARARGS | METH_KEYWORDS, NULL},
    {"compare_and_set",   (PyCFunction) AtomicDict_CompareAndSet_callable,  METH_VARARGS | METH_KEYWORDS, NULL},
    {"batch_getitem",     (PyCFunction) AtomicDict_BatchGetItem,            METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce",            (PyCFunction) AtomicDict_Reduce_callable,         METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_sum",        (PyCFunction) AtomicDict_ReduceSum_callable,      METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_and",        (PyCFunction) AtomicDict_ReduceAnd_callable,      METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_or",         (PyCFunction) AtomicDict_ReduceOr_callable,       METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_max",        (PyCFunction) AtomicDict_ReduceMax_callable,      METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_min",        (PyCFunction) AtomicDict_ReduceMin_callable,      METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_list",       (PyCFunction) AtomicDict_ReduceList_callable,     METH_VARARGS | METH_KEYWORDS, NULL},
    {"reduce_count",      (PyCFunction) AtomicDict_ReduceCount_callable,    METH_VARARGS | METH_KEYWORDS, NULL},
    {"get_handle",        (PyCFunction) AtomicDict_GetHandle,               METH_NOARGS, NULL},
    {"__class_getitem__", (PyCFunction) _generic_class_getitem,             METH_O | METH_CLASS, NULL},
    {NULL}
};

static PyMappingMethods AtomicDict_mapping_methods = {
    .mp_length = (lenfunc) AtomicDict_Len,
    .mp_subscript = (binaryfunc) AtomicDict_GetItem,
    .mp_ass_subscript = (objobjargproc) AtomicDict_SetItem,
};

PyTypeObject AtomicDict_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicDict",
    .tp_doc = PyDoc_STR("A thread-safe dictionary (hashmap), that's almost-lock-free™."),
    .tp_basicsize = sizeof(AtomicDict),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = AtomicDict_new,
    .tp_traverse = (traverseproc) AtomicDict_traverse,
    .tp_clear = (inquiry) AtomicDict_clear,
    .tp_dealloc = (destructor) AtomicDict_dealloc,
    .tp_init = (initproc) AtomicDict_init,
    .tp_methods = AtomicDict_methods,
    .tp_as_mapping = &AtomicDict_mapping_methods,
};

PyTypeObject AtomicDictMeta_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._AtomicDictMeta",
    .tp_basicsize = sizeof(AtomicDict_Meta),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = PyType_GenericNew,
    .tp_traverse = (traverseproc) AtomicDictMeta_traverse,
    .tp_clear = (inquiry) AtomicDictMeta_clear,
    .tp_dealloc = (destructor) AtomicDictMeta_dealloc,
};

PyTypeObject AtomicDictBlock_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._AtomicDictBlock",
    .tp_basicsize = sizeof(AtomicDict_Block),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = PyType_GenericNew,
    .tp_traverse = (traverseproc) AtomicDictBlock_traverse,
    .tp_clear = (inquiry) AtomicDictBlock_clear,
    .tp_dealloc = (destructor) AtomicDictBlock_dealloc,
};

PyTypeObject AtomicDictFastIterator_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._AtomicDictFastIterator",
    .tp_basicsize = sizeof(AtomicDict_FastIterator),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor) AtomicDictFastIterator_dealloc,
    .tp_iter = (getiterfunc) AtomicDictFastIterator_GetIter,
    .tp_iternext = (iternextfunc) AtomicDictFastIterator_Next,
};


static PyMethodDef AtomicEvent_methods[] = {
    {"wait",   (PyCFunction) AtomicEvent_Wait_callable,  METH_NOARGS, NULL},
    {"set",    (PyCFunction) AtomicEvent_Set_callable,   METH_NOARGS, NULL},
    {"is_set", (PyCFunction) AtomicEvent_IsSet_callable, METH_NOARGS, NULL},
    {NULL}
};

PyTypeObject AtomicEvent_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicEvent",
    .tp_doc = PyDoc_STR("An atomic event based on pthread's condition locks."),
    .tp_basicsize = sizeof(AtomicEvent),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor) AtomicEvent_dealloc,
    .tp_init = (initproc) AtomicEvent_init,
    .tp_methods = AtomicEvent_methods,
};


// see constants.h
PyObject *NOT_FOUND = NULL;
PyObject *ANY = NULL;
PyObject *EXPECTATION_FAILED = NULL;
PyObject *Cereggii_ExpectationFailed = NULL;

PyTypeObject CereggiiConstant_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.Constant",
    .tp_doc = PyDoc_STR("A constant value."),
    .tp_basicsize = sizeof(CereggiiConstant),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_repr = (reprfunc) CereggiiConstant_Repr,
};


static PyNumberMethods ThreadHandle_as_number = {
    .nb_add = (binaryfunc) ThreadHandle_Add,
    .nb_subtract = (binaryfunc) ThreadHandle_Subtract,
    .nb_multiply = (binaryfunc) ThreadHandle_Multiply,
    .nb_remainder = (binaryfunc) ThreadHandle_Remainder,
    .nb_divmod = (binaryfunc) ThreadHandle_Divmod,
    .nb_power = (ternaryfunc) ThreadHandle_Power,
    .nb_negative = (unaryfunc) ThreadHandle_Negative,
    .nb_positive = (unaryfunc) ThreadHandle_Positive,
    .nb_absolute = (unaryfunc) ThreadHandle_Absolute,
    .nb_bool = (inquiry) ThreadHandle_Bool,
    .nb_invert = (unaryfunc) ThreadHandle_Invert,
    .nb_lshift = (binaryfunc) ThreadHandle_Lshift,
    .nb_rshift = (binaryfunc) ThreadHandle_Rshift,
    .nb_and = (binaryfunc) ThreadHandle_And,
    .nb_xor = (binaryfunc) ThreadHandle_Xor,
    .nb_or = (binaryfunc) ThreadHandle_Or,
    .nb_int = (unaryfunc) ThreadHandle_Int,
    .nb_float = (unaryfunc) ThreadHandle_Float,

    .nb_inplace_add = (binaryfunc) ThreadHandle_InPlaceAdd,
    .nb_inplace_subtract = (binaryfunc) ThreadHandle_InPlaceSubtract,
    .nb_inplace_multiply = (binaryfunc) ThreadHandle_InPlaceMultiply,
    .nb_inplace_remainder = (binaryfunc) ThreadHandle_InPlaceRemainder,
    .nb_inplace_power = (ternaryfunc) ThreadHandle_InPlacePower,
    .nb_inplace_lshift = (binaryfunc) ThreadHandle_InPlaceLshift,
    .nb_inplace_rshift = (binaryfunc) ThreadHandle_InPlaceRshift,
    .nb_inplace_and = (binaryfunc) ThreadHandle_InPlaceAnd,
    .nb_inplace_xor = (binaryfunc) ThreadHandle_InPlaceXor,
    .nb_inplace_or = (binaryfunc) ThreadHandle_InPlaceOr,

    .nb_floor_divide = (binaryfunc) ThreadHandle_FloorDivide,
    .nb_true_divide = (binaryfunc) ThreadHandle_TrueDivide,
    .nb_inplace_floor_divide = (binaryfunc) ThreadHandle_InPlaceFloorDivide,
    .nb_inplace_true_divide = (binaryfunc) ThreadHandle_InPlaceTrueDivide,

    .nb_index = (unaryfunc) ThreadHandle_Index,

    .nb_matrix_multiply = (binaryfunc) ThreadHandle_MatrixMultiply,
    .nb_inplace_matrix_multiply = (binaryfunc) ThreadHandle_InPlaceMatrixMultiply,
};

static PySequenceMethods ThreadHandle_as_sequence = {
    .sq_length = (lenfunc) ThreadHandle_Length,
    .sq_concat = (binaryfunc) ThreadHandle_Concat,
    .sq_repeat = (ssizeargfunc) ThreadHandle_Repeat,
    .sq_item = (ssizeargfunc) ThreadHandle_GetItem,
    .sq_ass_item = (ssizeobjargproc) ThreadHandle_SetItem,
    .sq_contains = (objobjproc) ThreadHandle_Contains,
    .sq_inplace_concat = (binaryfunc) ThreadHandle_InPlaceConcat,
    .sq_inplace_repeat = (ssizeargfunc) ThreadHandle_InPlaceRepeat,
};

static PyMappingMethods ThreadHandle_as_mapping = {
    .mp_length = (lenfunc) ThreadHandle_MappingLength,
    .mp_subscript = (binaryfunc) ThreadHandle_MappingGetItem,
    .mp_ass_subscript = (objobjargproc) ThreadHandle_MappingSetItem,
};

// static PyAsyncMethods ThreadHandle_as_async = {
//     .am_await = (unaryfunc) ThreadHandle_Await,
//     .am_aiter = (unaryfunc) ThreadHandle_GetAIter,
//     .am_anext = (unaryfunc) ThreadHandle_AsyncNext,
//     .am_send = (sendfunc) ThreadHandle_AsyncSend,
// };

static PyMethodDef ThreadHandle_methods[] = {
    {"__class_getitem__", (PyCFunction) _generic_class_getitem, METH_O | METH_CLASS, NULL},
    {NULL}
};

PyTypeObject ThreadHandle_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.ThreadHandle",
    .tp_doc = PyDoc_STR("A thread-local handle for referencing a shared object."),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_basicsize = sizeof(ThreadHandle),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) ThreadHandle_init,
    .tp_traverse = (traverseproc) ThreadHandle_traverse,
    .tp_clear = (inquiry) ThreadHandle_clear,
    .tp_dealloc = (destructor) ThreadHandle_dealloc,
    .tp_richcompare = (richcmpfunc) ThreadHandle_RichCompare,
    .tp_hash = (hashfunc) ThreadHandle_Hash,
    .tp_as_number = &ThreadHandle_as_number,
    .tp_as_sequence = &ThreadHandle_as_sequence,
    .tp_as_mapping = &ThreadHandle_as_mapping,
    // .tp_as_async = &ThreadHandle_as_async,
    .tp_repr = (reprfunc) ThreadHandle_Repr,
    .tp_str = (reprfunc) ThreadHandle_Repr,
    .tp_call = (ternaryfunc) ThreadHandle_Call,
    .tp_iter = (getiterfunc) ThreadHandle_GetIter,
    // .tp_iternext = (iternextfunc) ThreadHandle_Next,
    .tp_methods = ThreadHandle_methods,
    .tp_getattro = (getattrofunc) ThreadHandle_GetAttr,
    .tp_setattro = (setattrofunc) ThreadHandle_SetAttr,
};


static PyModuleDef cereggii_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_cereggii",
    .m_doc = NULL,
    .m_size = -1,
};

__attribute__((unused)) PyMODINIT_FUNC
PyInit__cereggii(void)
{
    PyObject *m = NULL;

    if (PyType_Ready(&CereggiiConstant_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicDict_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicDictMeta_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicDictBlock_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicDictFastIterator_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicEvent_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicRef_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicInt64_Type) < 0)
        return NULL;
    if (PyType_Ready(&ThreadHandle_Type) < 0)
        return NULL;


    Cereggii_ExpectationFailed = PyErr_NewException("cereggii.ExpectationFailed", NULL, NULL);
    if (Cereggii_ExpectationFailed == NULL)
        return NULL;

    NOT_FOUND = CereggiiConstant_New("NOT_FOUND");
    if (NOT_FOUND == NULL)
        return NULL;

    ANY = CereggiiConstant_New("ANY");
    if (ANY == NULL)
        return NULL;

    EXPECTATION_FAILED = CereggiiConstant_New("EXPECTATION_FAILED");
    if (EXPECTATION_FAILED == NULL)
        return NULL;

    m = PyModule_Create(&cereggii_module);
    if (m == NULL)
        return NULL;

#ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(m, Py_MOD_GIL_NOT_USED);
#endif

    if (PyModule_AddObjectRef(m, "NOT_FOUND", NOT_FOUND) < 0)
        goto fail;
    Py_DECREF(NOT_FOUND);

    if (PyModule_AddObjectRef(m, "ANY", ANY) < 0)
        goto fail;
    Py_DECREF(ANY);

    if (PyModule_AddObjectRef(m, "EXPECTATION_FAILED", EXPECTATION_FAILED) < 0)
        goto fail;
    Py_DECREF(EXPECTATION_FAILED);

    if (PyModule_AddObjectRef(m, "ExpectationFailed", Cereggii_ExpectationFailed) < 0)
        goto fail;
    Py_DECREF(Cereggii_ExpectationFailed);

    if (PyModule_AddObjectRef(m, "AtomicDict", (PyObject *) &AtomicDict_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicDict_Type);

    if (PyModule_AddObjectRef(m, "AtomicEvent", (PyObject *) &AtomicEvent_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicEvent_Type);

    if (PyModule_AddObjectRef(m, "AtomicRef", (PyObject *) &AtomicRef_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicRef_Type);

    if (PyModule_AddObjectRef(m, "AtomicInt64", (PyObject *) &AtomicInt64_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicInt64_Type);

    if (PyModule_AddObjectRef(m, "ThreadHandle", (PyObject *) &ThreadHandle_Type) < 0)
        goto fail;
    Py_DECREF(&ThreadHandle_Type);

    return m;
    fail:
    Py_XDECREF(m);
    Py_XDECREF(NOT_FOUND);
    Py_XDECREF(ANY);
    Py_XDECREF(EXPECTATION_FAILED);
    return NULL;
}
