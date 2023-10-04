# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0


def test_nogil():
    import sys

    assert getattr(sys.flags, 'nogil', False)


def test_import():
    import cereggii

    assert type(getattr(cereggii, "__version__", None)) == str
    assert type(getattr(cereggii, "__version_tuple__", None)) == tuple
