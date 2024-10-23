# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

"""
Thread synchronization utilities for free-threaded Python.
"""

from .__about__ import __license__, __version__, __version_tuple__  # noqa: F401
from .atomic_dict import AtomicDict  # noqa: F401
from .atomic_int import AtomicInt64, AtomicInt64Handle  # noqa: F401
from .atomic_ref import AtomicRef  # noqa: F401
from .constants import *  # noqa: F403
