// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_int.h"
#include "atomic_ref.h"
#include "atomic_dict.h"


static PyMethodDef AtomicInt_methods[] = {
    {"get",               (PyCFunction) AtomicInt_Get_callable,             METH_NOARGS,  NULL},
    {"set",               (PyCFunction) AtomicInt_Set_callable,             METH_O,       NULL},
    {"compare_and_set",   (PyCFunction) AtomicInt_CompareAndSet_callable,   METH_VARARGS | METH_KEYWORDS, NULL},
    {"get_and_set",       (PyCFunction) AtomicInt_GetAndSet_callable,       METH_VARARGS | METH_KEYWORDS, NULL},
    {"increment_and_get", (PyCFunction) AtomicInt_IncrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_increment", (PyCFunction) AtomicInt_GetAndIncrement_callable, METH_VARARGS, NULL},
    {"decrement_and_get", (PyCFunction) AtomicInt_DecrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_decrement", (PyCFunction) AtomicInt_GetAndDecrement_callable, METH_VARARGS, NULL},
    {"update_and_get",    (PyCFunction) AtomicInt_UpdateAndGet_callable,    METH_O,       NULL},
    {"get_and_update",    (PyCFunction) AtomicInt_GetAndUpdate_callable,    METH_O,       NULL},
    {"get_handle",        (PyCFunction) AtomicInt_GetHandle,                METH_NOARGS,  NULL},
    {"as_integer_ratio",  (PyCFunction) AtomicInt_AsIntegerRatio,           METH_NOARGS,  NULL},
    {"bit_length",        (PyCFunction) AtomicInt_BitLength,                METH_NOARGS,  NULL},
    {"conjugate",         (PyCFunction) AtomicInt_Conjugate,                METH_NOARGS,  NULL},
    {"from_bytes",        (PyCFunction) AtomicInt_FromBytes,                METH_VARARGS | METH_KEYWORDS |
                                                                            METH_CLASS,                   NULL},
    {"to_bytes",          (PyCFunction) AtomicInt_ToBytes,                  METH_NOARGS,  NULL},
    {NULL}
};

static PyGetSetDef AtomicInt_properties[] = {
    {"denominator", (getter) AtomicInt_Denominator_Get, (setter) AtomicInt_Denominator_Set, NULL, NULL},
    {"numerator",   (getter) AtomicInt_Numerator_Get,   (setter) AtomicInt_Numerator_Set,   NULL, NULL},
    {"imag",        (getter) AtomicInt_Imag_Get,        (setter) AtomicInt_Imag_Set,        NULL, NULL},
    {"real",        (getter) AtomicInt_Real_Get,        (setter) AtomicInt_Real_Set,        NULL, NULL},
    {NULL},
};

static PyNumberMethods AtomicInt_as_number = {
    .nb_add = (binaryfunc) AtomicInt_Add,
    .nb_subtract = (binaryfunc) AtomicInt_Subtract,
    .nb_multiply = (binaryfunc) AtomicInt_Multiply,
    .nb_remainder = (binaryfunc) AtomicInt_Remainder,
    .nb_divmod = (binaryfunc) AtomicInt_Divmod,
    .nb_power = (ternaryfunc) AtomicInt_Power,
    .nb_negative = (unaryfunc) AtomicInt_Negative,
    .nb_positive = (unaryfunc) AtomicInt_Positive,
    .nb_absolute = (unaryfunc) AtomicInt_Absolute,
    .nb_bool = (inquiry) AtomicInt_Bool,
    .nb_invert = (unaryfunc) AtomicInt_Invert,
    .nb_lshift = (binaryfunc) AtomicInt_Lshift,
    .nb_rshift = (binaryfunc) AtomicInt_Rshift,
    .nb_and = (binaryfunc) AtomicInt_And,
    .nb_xor = (binaryfunc) AtomicInt_Xor,
    .nb_or = (binaryfunc) AtomicInt_Or,
    .nb_int = (unaryfunc) AtomicInt_Int,
    .nb_float = (unaryfunc) AtomicInt_Float,

    .nb_inplace_add = (binaryfunc) AtomicInt_InplaceAdd,
    .nb_inplace_subtract = (binaryfunc) AtomicInt_InplaceSubtract,
    .nb_inplace_multiply = (binaryfunc) AtomicInt_InplaceMultiply,
    .nb_inplace_remainder = (binaryfunc) AtomicInt_InplaceRemainder,
    .nb_inplace_power = (ternaryfunc) AtomicInt_InplacePower,
    .nb_inplace_lshift = (binaryfunc) AtomicInt_InplaceLshift,
    .nb_inplace_rshift = (binaryfunc) AtomicInt_InplaceRshift,
    .nb_inplace_and = (binaryfunc) AtomicInt_InplaceAnd,
    .nb_inplace_xor = (binaryfunc) AtomicInt_InplaceXor,
    .nb_inplace_or = (binaryfunc) AtomicInt_InplaceOr,

    .nb_floor_divide = (binaryfunc) AtomicInt_FloorDivide,
    .nb_true_divide = (binaryfunc) AtomicInt_TrueDivide,
    .nb_inplace_floor_divide = (binaryfunc) AtomicInt_InplaceFloorDivide,
    .nb_inplace_true_divide = (binaryfunc) AtomicInt_InplaceTrueDivide,

    .nb_index = (unaryfunc) AtomicInt_Index,

    .nb_matrix_multiply = (binaryfunc) AtomicInt_MatrixMultiply,
    .nb_inplace_matrix_multiply = (binaryfunc) AtomicInt_InplaceMatrixMultiply,
};

PyTypeObject AtomicInt_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicInt",
    .tp_doc = PyDoc_STR("An int that may be updated atomically."),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_basicsize = sizeof(AtomicInt),
    .tp_itemsize = 0,
    .tp_new = AtomicInt_new,
    .tp_init = (initproc) AtomicInt_init,
    .tp_dealloc = (destructor) AtomicInt_dealloc,
    .tp_methods = AtomicInt_methods,
    .tp_as_number = &AtomicInt_as_number,
    .tp_richcompare = (richcmpfunc) AtomicInt_RichCompare,
    .tp_hash = (hashfunc) AtomicInt_Hash,
    .tp_getset = AtomicInt_properties,
};

static PyMethodDef AtomicIntHandle_methods[] = {
    {"get",               (PyCFunction) AtomicIntHandle_Get_callable,             METH_NOARGS,  NULL},
    {"set",               (PyCFunction) AtomicIntHandle_Set_callable,             METH_O,       NULL},
    {"compare_and_set",   (PyCFunction) AtomicIntHandle_CompareAndSet_callable,   METH_VARARGS | METH_KEYWORDS, NULL},
    {"get_and_set",       (PyCFunction) AtomicIntHandle_GetAndSet_callable,       METH_VARARGS | METH_KEYWORDS, NULL},
    {"increment_and_get", (PyCFunction) AtomicIntHandle_IncrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_increment", (PyCFunction) AtomicIntHandle_GetAndIncrement_callable, METH_VARARGS, NULL},
    {"decrement_and_get", (PyCFunction) AtomicIntHandle_DecrementAndGet_callable, METH_VARARGS, NULL},
    {"get_and_decrement", (PyCFunction) AtomicIntHandle_GetAndDecrement_callable, METH_VARARGS, NULL},
    {"update_and_get",    (PyCFunction) AtomicIntHandle_UpdateAndGet_callable,    METH_O,       NULL},
    {"get_and_update",    (PyCFunction) AtomicIntHandle_GetAndUpdate_callable,    METH_O,       NULL},
    {"get_handle",        (PyCFunction) AtomicIntHandle_GetHandle,                METH_NOARGS,  NULL},
    {"as_integer_ratio",  (PyCFunction) AtomicIntHandle_AsIntegerRatio,           METH_NOARGS,  NULL},
    {"bit_length",        (PyCFunction) AtomicIntHandle_BitLength,                METH_NOARGS,  NULL},
    {"conjugate",         (PyCFunction) AtomicIntHandle_Conjugate,                METH_NOARGS,  NULL},
    {"from_bytes",        (PyCFunction) AtomicIntHandle_FromBytes,                METH_VARARGS | METH_KEYWORDS |
                                                                                  METH_CLASS,                   NULL},
    {"to_bytes",          (PyCFunction) AtomicIntHandle_ToBytes,                  METH_NOARGS,  NULL},
    {NULL}
};

static PyGetSetDef AtomicIntHandle_properties[] = {
    {"denominator", (getter) AtomicIntHandle_Denominator_Get, (setter) AtomicIntHandle_Denominator_Set, NULL, NULL},
    {"numerator",   (getter) AtomicIntHandle_Numerator_Get,   (setter) AtomicIntHandle_Numerator_Set,   NULL, NULL},
    {"imag",        (getter) AtomicIntHandle_Imag_Get,        (setter) AtomicIntHandle_Imag_Set,        NULL, NULL},
    {"real",        (getter) AtomicIntHandle_Real_Get,        (setter) AtomicIntHandle_Real_Set,        NULL, NULL},
    {NULL},
};

static PyNumberMethods AtomicIntHandle_as_number = {
    .nb_add = (binaryfunc) AtomicIntHandle_Add,
    .nb_subtract = (binaryfunc) AtomicIntHandle_Subtract,
    .nb_multiply = (binaryfunc) AtomicIntHandle_Multiply,
    .nb_remainder = (binaryfunc) AtomicIntHandle_Remainder,
    .nb_divmod = (binaryfunc) AtomicIntHandle_Divmod,
    .nb_power = (ternaryfunc) AtomicIntHandle_Power,
    .nb_negative = (unaryfunc) AtomicIntHandle_Negative,
    .nb_positive = (unaryfunc) AtomicIntHandle_Positive,
    .nb_absolute = (unaryfunc) AtomicIntHandle_Absolute,
    .nb_bool = (inquiry) AtomicIntHandle_Bool,
    .nb_invert = (unaryfunc) AtomicIntHandle_Invert,
    .nb_lshift = (binaryfunc) AtomicIntHandle_Lshift,
    .nb_rshift = (binaryfunc) AtomicIntHandle_Rshift,
    .nb_and = (binaryfunc) AtomicIntHandle_And,
    .nb_xor = (binaryfunc) AtomicIntHandle_Xor,
    .nb_or = (binaryfunc) AtomicIntHandle_Or,
    .nb_int = (unaryfunc) AtomicIntHandle_Int,
    .nb_float = (unaryfunc) AtomicIntHandle_Float,

    .nb_inplace_add = (binaryfunc) AtomicIntHandle_InplaceAdd,
    .nb_inplace_subtract = (binaryfunc) AtomicIntHandle_InplaceSubtract,
    .nb_inplace_multiply = (binaryfunc) AtomicIntHandle_InplaceMultiply,
    .nb_inplace_remainder = (binaryfunc) AtomicIntHandle_InplaceRemainder,
    .nb_inplace_power = (ternaryfunc) AtomicIntHandle_InplacePower,
    .nb_inplace_lshift = (binaryfunc) AtomicIntHandle_InplaceLshift,
    .nb_inplace_rshift = (binaryfunc) AtomicIntHandle_InplaceRshift,
    .nb_inplace_and = (binaryfunc) AtomicIntHandle_InplaceAnd,
    .nb_inplace_xor = (binaryfunc) AtomicIntHandle_InplaceXor,
    .nb_inplace_or = (binaryfunc) AtomicIntHandle_InplaceOr,

    .nb_floor_divide = (binaryfunc) AtomicIntHandle_FloorDivide,
    .nb_true_divide = (binaryfunc) AtomicIntHandle_TrueDivide,
    .nb_inplace_floor_divide = (binaryfunc) AtomicIntHandle_InplaceFloorDivide,
    .nb_inplace_true_divide = (binaryfunc) AtomicIntHandle_InplaceTrueDivide,

    .nb_index = (unaryfunc) AtomicIntHandle_Index,

    .nb_matrix_multiply = (binaryfunc) AtomicIntHandle_MatrixMultiply,
    .nb_inplace_matrix_multiply = (binaryfunc) AtomicIntHandle_InplaceMatrixMultiply,
};

PyTypeObject AtomicIntHandle_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicIntHandle",
    .tp_doc = PyDoc_STR("An immutable handle for referencing an AtomicInt."),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_basicsize = sizeof(AtomicIntHandle),
    .tp_itemsize = 0,
    .tp_new = AtomicIntHandle_new,
    .tp_init = (initproc) AtomicIntHandle_init,
    .tp_dealloc = (destructor) AtomicIntHandle_dealloc,
    .tp_methods = AtomicIntHandle_methods,
    .tp_as_number = &AtomicIntHandle_as_number,
    .tp_richcompare = (richcmpfunc) AtomicIntHandle_RichCompare,
    .tp_hash = (hashfunc) AtomicIntHandle_Hash,
    .tp_getset = AtomicIntHandle_properties,
};


static PyMethodDef AtomicRef_methods[] = {
    {"get",             (PyCFunction) AtomicRef_Get,                    METH_NOARGS,  NULL},
    {"set",             (PyCFunction) AtomicRef_Set,                    METH_O,       NULL},
    {"compare_and_set", (PyCFunction) AtomicRef_CompareAndSet_callable, METH_VARARGS, NULL},
    {"get_and_set",     (PyCFunction) AtomicRef_GetAndSet,              METH_O,       NULL},
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
    {"debug", (PyCFunction) AtomicDict_Debug,                   METH_NOARGS, NULL},
    {"get",   (PyCFunction) AtomicDict_GetItemOrDefaultVarargs, METH_VARARGS | METH_KEYWORDS, NULL},
    {NULL}
};

static PyMappingMethods AtomicDict_mapping_methods = {
    .mp_length = (lenfunc) NULL, // atomic_dict_imprecise_length / atomic_dict_precise_length
    .mp_subscript = (binaryfunc) AtomicDict_GetItem,
    .mp_ass_subscript = (objobjargproc) AtomicDict_SetItem,
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
    if (PyType_Ready(&AtomicInt_Type) < 0)
        return NULL;
    if (PyType_Ready(&AtomicIntHandle_Type) < 0)
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

    Py_INCREF(&AtomicInt_Type);
    if (PyModule_AddObject(m, "AtomicInt", (PyObject *) &AtomicInt_Type) < 0) {
        Py_DECREF(&AtomicInt_Type);
        goto fail;
    }

    Py_INCREF(&AtomicIntHandle_Type);
    if (PyModule_AddObject(m, "AtomicIntHandle", (PyObject *) &AtomicIntHandle_Type) < 0) {
        Py_DECREF(&AtomicIntHandle_Type);
        goto fail;
    }

    return m;
    fail:
    Py_DECREF(m);
    return NULL;
}
