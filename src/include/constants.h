// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_CONSTANTS_H
#define CEREGGII_CONSTANTS_H

#include "Python.h"

extern PyObject *NOT_FOUND;
extern PyObject *ANY;
extern PyObject *EXPECTATION_FAILED;
extern PyObject *Cereggii_ExpectationFailed;


typedef struct CereggiiConstant {
    PyObject_HEAD

    char *name;
} CereggiiConstant;

extern PyTypeObject CereggiiConstant_Type;


PyObject *CereggiiConstant_New(char *name);

PyObject *CereggiiConstant_Repr(CereggiiConstant *self);

#endif //CEREGGII_CONSTANTS_H
