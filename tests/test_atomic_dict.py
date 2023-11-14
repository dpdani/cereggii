# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import gc

from cereggii import AtomicDict
from pytest import raises


def test_init():
    AtomicDict()
    AtomicDict(
        {
            "spam": 42,
            3: 4,
        }
    )
    AtomicDict(spam=42, cheese=True)
    AtomicDict(
        {
            "bird": "norwegian blue",
        },
        bird="spam",
    )
    AtomicDict(initial_size=120)


def test_log_size_bumped():
    keys = [i * 64 for i in range(10)]
    d = AtomicDict({k: None for k in keys})
    for k in keys:
        assert d[k] is None

    keys = [i * 64 for i in range(26)]
    d = AtomicDict({k: None for k in keys})
    for k in keys:
        assert d[k] is None


def test_key_error():
    d = AtomicDict()
    with raises(KeyError):
        # noinspection PyStatementEffect
        d[0]


def test_getitem():
    d = AtomicDict({"spam": 42})
    assert d["spam"] == 42
    c = AtomicDict({"spam": 42}, spam=43)
    assert c["spam"] == 43


def test_setitem_updates_a_value_set_at_init():
    d = AtomicDict({0: 1})
    d[0] = 3
    assert d[0] == 3


def test_setitem_inserts_a_value():
    d = AtomicDict(initial_size=64 * 4)
    d[0] = 42
    d[2] = 2
    assert d[0] == 42
    assert d[2] == 2


def test_setitem_updates_an_inserted_value():
    d = AtomicDict()
    d[0] = 1
    assert d[0] == 1
    d[0] = 2
    assert d[0] == 2


# def test_setitem_distance_1_insert():
#     d = AtomicDict({0: 1})
#     d[64] = 42
#     assert d[64] == 42


def test_dealloc():
    d = AtomicDict({"spam": 42})
    del d
    gc.collect()
