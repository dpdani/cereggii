# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import gc
import threading

import pytest
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
        d[0]


def test_getitem():
    d = AtomicDict({"spam": 42})
    assert d["spam"] == 42
    c = AtomicDict({"spam": 42}, spam=43)
    assert c["spam"] == 43


def test_getitem_confused():
    d = AtomicDict()
    d[0] = 1
    assert d[0] == 1
    d[64] = 2
    assert d[64] == 2
    d[128] = 3
    assert d[128] == 3
    with raises(KeyError):
        d[256]


def test_setitem_updates_a_value_set_at_init():
    d = AtomicDict({0: 1})
    d[0] = 3
    assert d[0] == 3


def test_setitem_inserts_a_value():
    d = AtomicDict(initial_size=64 * 4 - 1)
    d[0] = 42
    d[2] = 2
    d[128] = 1
    assert d[0] == 42
    assert d[2] == 2
    assert d[128] == 1


def test_setitem_updates_an_inserted_value():
    d = AtomicDict()
    d[0] = 1
    assert d[0] == 1
    d[0] = 2
    assert d[0] == 2


def test_setitem_distance_1_insert():
    d = AtomicDict({0: 1})
    assert d[0] == 1
    d[64] = 42
    assert d[0] == 1
    assert d[64] == 42
    assert d.debug()["index"][1] == 10
    d = AtomicDict()
    d[0] = 1
    assert d[0] == 1
    d[1] = 2
    assert d[0] == 1
    assert d[1] == 2
    d[64] = 3
    assert d[0] == 1
    assert d[1] == 2
    assert d[64] == 3
    assert d.debug()["index"][0] == 7
    assert d.debug()["index"][1] == 14
    assert d.debug()["index"][2] == 10


@pytest.mark.skip()
def test_insert_with_reservation():
    d = AtomicDict({
        k: None
        for k in range(16)
    })
    # breakpoint()
    d[64] = 1
    for k in range(16):
        assert d[k] is None
    assert d[64] == 1


def test_dealloc():
    d = AtomicDict({"spam": 42})
    del d
    gc.collect()


def test_concurrent_insert():
    d = AtomicDict(initial_size=64 * 2)

    def thread_1():
        d[0] = 1
        d[1] = 1
        d[2] = 1

    def thread_2():
        d[3] = 2
        d[4] = 2
        d[2] = 2

    t1 = threading.Thread(target=thread_1)
    t2 = threading.Thread(target=thread_2)

    t1.start()
    t2.start()
    t1.join()
    t2.join()

    assert d[0] == 1
    assert d[1] == 1

    assert d[3] == 2
    assert d[4] == 2

    assert d[2] in (1, 2)
