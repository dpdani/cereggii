# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0


def test_nogil():
    import sys

    assert not sys._is_gil_enabled()


def test_import():
    import cereggii

    assert isinstance(getattr(cereggii, "__version__", None), str)
    assert isinstance(getattr(cereggii, "__version_tuple__", None), tuple)
