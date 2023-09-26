# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

__version__ = "0.0.1"

major, minor, patch, *_ = __version__.split(".")

__version_tuple__ = (int(major), int(minor), int(patch))

__license__ = "Apache-2.0"


__all__ = [
    "__version__",
    "__version_tuple__",
    "__license__",
]