# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import warnings


try:
    from cereggii import _cereggii
except ImportError as exc:  # building sdist (without compiled modules)

    class AtomicDict:
        def __init__(self):
            print("dummy")

    warnings.warn(str(exc), stacklevel=1)  # "UserWarning: No module named 'cereggii'" is expected during sdist build

else:
    AtomicDict = _cereggii.AtomicDict
