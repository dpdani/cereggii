# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import cereggii
from pytest import raises


def test_constants():
    assert repr(cereggii.ANY)
    assert repr(cereggii.NOT_FOUND)
    assert repr(cereggii.EXPECTATION_FAILED)


def test_cannot_instantiate_constants():
    # https://github.com/dpdani/cereggii/issues/73
    with raises(RuntimeError):
        repr(cereggii.ANY.__class__())
