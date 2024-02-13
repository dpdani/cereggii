# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import gc
import threading
from collections import Counter

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
    AtomicDict(min_size=120)


def test_weird_init():
    # this may be confusing: the iterable is the input parameter for initializing
    # the dictionary, but it is a positional-only parameter. thus, when passing it
    # as a keyword parameter, it is considered to be one key in itself.
    d = AtomicDict(min_size=64, iterable={1: 0})
    with raises(KeyError):
        assert d[1] == 0
    assert d["iterable"] == {1: 0}

    with raises(TypeError):
        AtomicDict(None)


def test_debug():
    d = AtomicDict({"key": "value"}, min_size=64, iterable={1: 0})
    dbg = d.debug()
    assert type(dbg) is dict  # noqa: E721
    assert "meta" in dbg
    assert "index" in dbg
    assert "blocks" in dbg
    del dbg
    previous = None
    while (this := gc.collect()) != previous:
        previous = this
    d.debug()
    previous = None
    while (this := gc.collect()) != previous:
        previous = this


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
    d = AtomicDict(min_size=64 - 1)
    d[0] = 42
    d[2] = 2
    d[67108864] = 1  # 67108864 = 1 << 26 => hash(67108864) == hash(0)
    assert d[0] == 42
    assert d[2] == 2
    assert d[67108864] == 1
    d = AtomicDict(min_size=(1 << 7) - 1)
    d[0] = 42
    d[2] = 2
    d[67108864] = 1
    assert d[0] == 42
    assert d[2] == 2
    assert d[67108864] == 1
    d = AtomicDict(min_size=(1 << 11) - 1)
    d[0] = 42
    d[2] = 2
    d[67108864] = 1
    assert d[0] == 42
    assert d[2] == 2
    assert d[67108864] == 1
    # breakpoint()  # 64 bit nodes not supported yet
    # d = AtomicDict(min_size=(1 << 26) - 1)
    # d[0] = 42
    # d[2] = 2
    # d[67108864] = 1
    # assert d[0] == 42
    # assert d[2] == 2
    # assert d[67108864] == 1


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


def test_insert_non_compact():
    d = AtomicDict({k: None for k in range(60)})
    d[64] = 1
    for k in range(60):
        assert d[k] is None
    assert d[64] == 1


def test_full_dict():
    d = AtomicDict({k: None for k in range(63)})
    assert len(d.debug()["index"]) == 64
    d = AtomicDict(min_size=64)
    for k in range(62):
        d[k] = None
    assert len(d.debug()["index"]) == 64

    d = AtomicDict({k: None for k in range((1 << 10) - 1)})
    assert len(d.debug()["index"]) == 1 << 10
    d = AtomicDict(min_size=1 << 10)
    for k in range((1 << 10) - 2):
        d[k] = None
    assert len(d.debug()["index"]) == 1 << 10


@pytest.mark.skip()
def test_full_dict_32():
    # this test is slow (allocates a lot of memory)
    d = AtomicDict({k: None for k in range((1 << 25) - 1)})
    assert len(d.debug()["index"]) == 1 << 25
    d = AtomicDict(min_size=1 << 25)
    for k in range((1 << 25) - 2):
        d[k] = None
    assert len(d.debug()["index"]) == 1 << 25


def test_node_size_64_unavailable():
    # see https://github.com/dpdani/cereggii/issues/3
    with raises(NotImplementedError):
        AtomicDict(min_size=1 << 26)


# def test_insert_with_reservation_64():
#     d = AtomicDict({k: None for k in range(16)}, min_size=(1 << 26) - 1)
#     breakpoint()
#     d[1 << 26] = 1
#     for k in range(16):
#         assert d[k] is None
#     assert d[64] == 1


def test_dealloc():
    d = AtomicDict({"spam": 42})
    del d
    previous = None
    while (this := gc.collect()) != previous:
        previous = this


def test_concurrent_insert():
    d = AtomicDict(min_size=64 * 2)

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


def test_get_default():
    d = AtomicDict()
    d["key"] = "value"
    assert d.get(1) is None
    assert d.get(1, "default") == "default"
    assert d.get("key") == "value"


def test_delete():
    d = AtomicDict({"spam": "lovely", "atomic": True})
    del d["atomic"]
    with raises(KeyError):
        del d["atomic"]

    d["flower"] = "cereus greggii"
    del d["flower"]
    with raises(KeyError):
        del d["flower"]

    d = AtomicDict({_: None for _ in range(15)})
    assert d.debug()["blocks"][0]["entries"][14]  # exists
    del d[14]
    with raises(KeyError):
        d[14]
    debug = d.debug()
    assert debug["index"][14] == 0
    with raises(IndexError):
        debug["blocks"][0]["entries"][14]

    d = AtomicDict({_: None for _ in range(16)})
    del d[14]
    with raises(KeyError):
        d[14]
    debug = d.debug()
    assert debug["index"][14] == 0

    d = AtomicDict({_: None for _ in range(15)})
    d[64 + 14] = None
    del d[14]
    with raises(KeyError):
        d[14]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({_: None for _ in range(15)})
    del d[7]
    with raises(KeyError):
        d[7]
    debug = d.debug()
    assert debug["index"][7] == 0

    d = AtomicDict({_: None for _ in range(8)})
    for _ in range(63 + 8, 63 + 8 + 7):
        d[_] = None
    debug = d.debug()
    assert debug["index"][14] != 0
    del d[7]
    with raises(KeyError):
        d[7]
    debug = d.debug()
    assert debug["index"][14] == 0

    d = AtomicDict({_: None for _ in range(16)})
    del d[15]
    with raises(KeyError):
        d[15]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({_: None for _ in range(17)})
    del d[15]
    with raises(KeyError):
        d[15]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({16: None})
    del d[16]
    with raises(KeyError):
        d[16]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({16: None, 17: None})
    del d[16]
    with raises(KeyError):
        d[16]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({15: None, 16: None})
    del d[16]
    with raises(KeyError):
        d[16]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({15: None, 16: None, 17: None})
    del d[16]
    with raises(KeyError):
        d[16]
    debug = d.debug()
    assert debug["index"][16] == 0


def test_delete_with_swap():
    d = AtomicDict({_: None for _ in range(64)} | {_: None for _ in range(64, 128)})
    del d[64]
    with raises(KeyError):
        del d[64]
    debug = d.debug()
    assert debug["index"][64] == 0
    assert debug["index"][0] >> (debug["meta"]["node_size"] - debug["meta"]["log_size"]) == 65
    assert debug["blocks"][1]["entries"][1] == (65, 128, 0, 0, None)


def test_delete_concurrent():
    d = AtomicDict({"spam": "lovely", "atomic": True, "flower": "cereus greggii"})

    key_error_1 = False
    key_error_2 = False

    def thread_1():
        del d["spam"]
        try:
            del d["flower"]
        except KeyError:
            nonlocal key_error_1
            key_error_1 = True

    def thread_2():
        del d["atomic"]
        try:
            del d["flower"]
        except KeyError:
            nonlocal key_error_2
            key_error_2 = True

    t1 = threading.Thread(target=thread_1)
    t2 = threading.Thread(target=thread_2)

    t1.start()
    t2.start()
    t1.join()
    t2.join()

    with raises(KeyError):
        d["spam"]
    with raises(KeyError):
        d["atomic"]
    with raises(KeyError):
        d["flower"]

    assert key_error_1 or key_error_2


def test_memory_leak():
    import tracemalloc

    tracemalloc.start()

    for _ in range(100):
        AtomicDict({"spam": "lovely", "atomic": True, "flower": "cereus greggii"})

    snap = tracemalloc.take_snapshot()
    tracemalloc.stop()

    snap = snap.statistics("traceback")
    statistics = []
    for stat in snap:
        tb = stat.traceback[-1]
        if tb.filename == __file__:
            statistics.append(stat)

    assert len(statistics) == 0


def test_grow():
    d = AtomicDict({0: None, 1: None, 64: None, 65: None})
    assert d.debug()["meta"]["log_size"] == 6
    assert len(list(filter(lambda _: _ != 0, Counter(d.debug()["index"]).keys()))) == len({0, 1, 64, 65})
    d[128] = None
    assert d.debug()["meta"]["log_size"] == 7
    nodes = Counter(d.debug()["index"])
    assert len(list(filter(lambda _: _ != 0, nodes.keys()))) == len({0, 1, 64, 65, 128})
    for _ in nodes:
        if _ != 0:
            assert nodes[_] == 1
    for _ in [0, 1, 64, 65, 128]:
        assert d[_] is None

    d = AtomicDict({_: None for _ in range(63)})
    assert d.debug()["meta"]["log_size"] == 6
    d[128] = None
    assert d.debug()["meta"]["log_size"] == 7
    for _ in [*range(63), 128]:
        assert d[_] is None


def test_compact():
    d = AtomicDict({0: None, 1: None, 64: None, 65: None})
    for _ in range(20):
        d[2**_] = None
    for _ in [0, 1, 64, 65]:
        assert d[_] is None
    for _ in range(20):
        assert d[2**_] is None
    debug_before = d.debug()
    assert len(list(filter(lambda _: _ != 0, Counter(debug_before["index"]).keys()))) == len(
        {0, 1, 64, 65, *[2**_ for _ in range(20)]}
    )
    assert not debug_before["meta"]["is_compact"]
    # 8192 == 2 ** 13 will be in a reservation
    entry_of_8192 = list(filter(lambda _: _[3] == 8192, debug_before["blocks"][0]["entries"]))
    assert len(entry_of_8192) == 1
    entry_of_8192 = entry_of_8192[0]
    non_empty_nodes = len(list(filter(lambda _: _ != 0, debug_before["index"])))
    node_of_8192 = list(
        filter(
            lambda _: (_ & debug_before["meta"]["index_mask"])
            >> (debug_before["meta"]["node_size"] - debug_before["meta"]["log_size"])
            == entry_of_8192[0],
            debug_before["index"],
        )
    )
    assert len(node_of_8192) == 1
    node_of_8192 = node_of_8192[0]
    assert node_of_8192 & debug_before["meta"]["distance_mask"] == 0
    d.compact()
    debug_after = d.debug()
    assert len(list(filter(lambda _: _ != 0, debug_after["index"]))) == non_empty_nodes
    assert len(list(filter(lambda _: _ != 0, Counter(debug_before["index"]).keys()))) == len(
        {0, 1, 64, 65, *[2**_ for _ in range(20)]}
    )
    assert debug_after["meta"]["is_compact"]
    entry_of_8192 = list(filter(lambda _: _[3] == 8192, debug_after["blocks"][0]["entries"]))
    assert len(entry_of_8192) == 1
    entry_of_8192 = entry_of_8192[0]
    node_of_8192 = list(
        filter(
            lambda _: (_ & debug_after["meta"]["index_mask"])
            >> (debug_after["meta"]["node_size"] - debug_after["meta"]["log_size"])
            == entry_of_8192[0],
            debug_after["index"],
        )
    )
    assert len(node_of_8192) == 1
    node_of_8192 = node_of_8192[0]
    assert node_of_8192 & debug_after["meta"]["distance_mask"] != 0

    d = AtomicDict({0: None, 1: None})
    for _ in range(20):
        d[2**_] = None
    d.compact()

    d = AtomicDict({}, min_size=2**16)
    assert len(d.debug()["index"]) == 2**16
    d.compact()
    assert len(d.debug()["index"]) == 2**16


def test_grow_then_shrink():
    d = AtomicDict()
    assert d.debug()["meta"]["log_size"] == 6

    for _ in range(2**10):
        assert len(list(filter(lambda _: _ != 0, Counter(d.debug()["index"]).keys()))) == _
        d[_] = None
    assert d.debug()["meta"]["log_size"] == 11

    for _ in range(2**10):
        # first shrink at _ == 766
        del d[_]
    assert d.debug()["meta"]["log_size"] == 7  # cannot shrink back to 6

    for _ in range(2**20, 2**20 + 2**14):
        d[_] = None

    assert d.debug()["meta"]["log_size"] == 15

    for _ in range(2**20, 2**20 + 2**14):
        del d[_]
    assert d.debug()["meta"]["log_size"] == 7


@pytest.mark.skip()
def test_large_grow_then_shrink():
    d = AtomicDict()
    assert d.debug()["meta"]["log_size"] == 6

    for _ in range(2**24):
        d[_] = None
    assert d.debug()["meta"]["log_size"] == 25

    for _ in range(2**24):
        del d[_]
    assert d.debug()["meta"]["log_size"] == 7


@pytest.mark.skip()
def test_large_pre_allocated_grow_then_shrink():
    d = AtomicDict(min_size=2**21)
    assert d.debug()["meta"]["log_size"] == 21

    for _ in range(2**20):
        d[_] = None
    assert d.debug()["meta"]["log_size"] == 21

    for _ in range(2**20):
        del d[_]
    assert d.debug()["meta"]["log_size"] == 21


def test_dont_implode():
    d = AtomicDict({1: None, 10: None})
    del d[1]
    d[2] = None
    assert d[2] is None
    assert d.debug()["meta"]["log_size"] == 6
