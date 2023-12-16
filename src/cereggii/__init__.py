# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

"""

Concurrent threading utilities for Python

"""

import sys
import warnings

from .__about__ import __license__, __version__, __version_tuple__  # noqa: F401
from .atomic_dict import AtomicDict  # noqa: F401
from .atomic_int import AtomicInt, AtomicIntHandle  # noqa: F401
from .atomic_ref import AtomicRef  # noqa: F401


if not getattr(sys.flags, "nogil", False):
    warnings.warn(
        "this library is meant to be used with nogil python: https://github.com/colesbury/nogil", stacklevel=1
    )
