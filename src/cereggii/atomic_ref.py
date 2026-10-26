# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import warnings

try:
    from cereggii import _cereggii  # noqa: F401
except ImportError as exc:  # building sdist (without compiled modules)

    class AtomicRef:
        def __init__(self):
            print("dummy")

    warnings.warn(str(exc), stacklevel=1)  # "UserWarning: No module named 'cereggii'" is expected during sdist build

else:
    from ._cereggii import AtomicRef  # noqa: F401
