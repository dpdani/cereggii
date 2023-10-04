# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0


from pytest import raises

from cereggii import AtomicDict


def test_key_error():
    d = AtomicDict()
    with raises(KeyError):
        # noinspection PyStatementEffect
        d[0]
