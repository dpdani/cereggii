// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "constants.h"


PyObject *
CereggiiConstant_New(char *name)
{
    CereggiiConstant *constant = NULL;
    constant = PyObject_New(CereggiiConstant, &CereggiiConstant_Type);

    if (constant == NULL)
        goto fail;

    constant->name = name;
#ifdef Py_GIL_DISABLED
    constant->ob_base.ob_ref_local = _Py_IMMORTAL_REFCNT_LOCAL;
#endif

    Py_INCREF(constant);
    return (PyObject *) constant;
    fail:
    return NULL;
}

int
CereggiiConstant_init(CereggiiConstant *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    PyErr_SetString(PyExc_RuntimeError, "cannot instantiate cereggii.Constant.");
    return -1;
}

PyObject *
CereggiiConstant_Repr(CereggiiConstant *self)
{
    return PyUnicode_FromFormat("<cereggii.%s>", self->name);
}
