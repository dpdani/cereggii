# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import sys
import warnings

from .__about__ import *
from .atomic_dict import AtomicDict
from .atomic_ref import AtomicRef


if not getattr(sys.flags, 'nogil', False):
    warnings.warn("this library is meant to be used with nogil python: https://github.com/colesbury/nogil")
