# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import gc
import random
import sys
import threading
from collections import Counter
from pprint import pprint

import pytest
import cereggii
from cereggii import AtomicDict
from pytest import raises


# class FixedHash:
#     def __init__(self, value: int, log_size: int = 6):
#         self.value = value
#         self.hash_value = (value << (64 - log_size)) & ((1 << sys.hash_info.width) - 1)
#         self.log_size = log_size
#
#     def __hash__(self):
#         return self.hash_value
#
#     def __eq__(self, other):
#         return isinstance(other, self.__class__) and other.value == self.value
#
#     def __repr__(self):
#         return f"<{self.__class__.__name__}({self.hash_value} <- {self.hash_value >> (64 - self.log_size)})>"


def FixedHash(i):
    return i


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
    keys = [FixedHash(i * 64) for i in range(10)]
    d = AtomicDict({k: None for k in keys})
    for k in keys:
        assert d[k] is None

    keys = [FixedHash(i * 64) for i in range(26)]
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
    d[FixedHash(0)] = 1
    assert d[FixedHash(0)] == 1
    d[FixedHash(64)] = 2
    assert d[FixedHash(64)] == 2
    d[FixedHash(128)] = 3
    assert d[FixedHash(128)] == 3
    with raises(KeyError):
        d[FixedHash(256)]


def test_setitem_updates_a_value_set_at_init():
    d = AtomicDict({FixedHash(0): 1})
    d[FixedHash(0)] = 3
    assert d[FixedHash(0)] == 3


def test_setitem_inserts_a_value():
    d = AtomicDict(min_size=64 - 1)
    d[FixedHash(0)] = 42
    d[FixedHash(2)] = 2
    d[FixedHash(67108864)] = 1  # 67108864 = 1 << 26 => hash(67108864) == hash(0)
    assert d[FixedHash(0)] == 42
    assert d[FixedHash(2)] == 2
    assert d[FixedHash(67108864)] == 1
    d = AtomicDict(min_size=(1 << 7) - 1)
    d[FixedHash(0)] = 42
    d[FixedHash(2)] = 2
    d[FixedHash(67108864)] = 1
    assert d[FixedHash(0)] == 42
    assert d[FixedHash(2)] == 2
    assert d[FixedHash(67108864)] == 1
    d = AtomicDict(min_size=(1 << 11) - 1)
    d[FixedHash(0)] = 42
    d[FixedHash(2)] = 2
    d[FixedHash(67108864)] = 1
    assert d[FixedHash(0)] == 42
    assert d[FixedHash(2)] == 2
    assert d[FixedHash(67108864)] == 1
    # breakpoint()  # 64 bit nodes not supported yet
    d = AtomicDict(min_size=(1 << 26) - 1)
    d[0] = 42
    d[2] = 2
    d[67108864] = 1
    assert d[0] == 42
    assert d[2] == 2
    assert d[67108864] == 1


def test_setitem_updates_an_inserted_value():
    d = AtomicDict()
    d[FixedHash(0)] = 1
    assert d[FixedHash(0)] == 1
    d[FixedHash(0)] = 2
    assert d[FixedHash(0)] == 2


def test_setitem_distance_1_insert():
    d = AtomicDict({FixedHash(0): 1})
    assert d[FixedHash(0)] == 1
    d[FixedHash(64)] = 42
    assert d[FixedHash(0)] == 1
    assert d[FixedHash(64)] == 42
    debug = d.debug()
    assert debug["index"][1] == 10
    d = AtomicDict()
    d[FixedHash(0)] = 1
    assert d[FixedHash(0)] == 1
    d[FixedHash(1)] = 2
    assert d[FixedHash(0)] == 1
    assert d[FixedHash(1)] == 2
    d[FixedHash(64)] = 3
    assert d[FixedHash(0)] == 1
    assert d[FixedHash(1)] == 2
    assert d[FixedHash(64)] == 3
    debug = d.debug()
    assert debug["index"][0] == 7
    assert debug["index"][1] == 14
    assert debug["index"][2] == 10


def test_insert_non_compact():
    d = AtomicDict({FixedHash(k): None for k in range(60)})
    d[FixedHash(64)] = 1
    for k in range(60):
        assert d[FixedHash(k)] is None
    assert d[FixedHash(64)] == 1


def test_full_dict():
    d = AtomicDict({FixedHash(k): None for k in range(63)})
    assert len(d.debug()["index"]) == 128

    d = AtomicDict(min_size=64)
    for k in range(62):
        d[FixedHash(k)] = None
    assert len(d.debug()["index"]) == 64

    d = AtomicDict({FixedHash(k): None for k in range((1 << 10) - 1)})
    assert len(d.debug()["index"]) == 1 << 11
    d = AtomicDict(min_size=1 << 10)
    for k in range((1 << 10) - 2):
        d[FixedHash(k)] = None
    assert len(d.debug()["index"]) == 1 << 11


@pytest.mark.skip()
def test_full_dict_32():
    # this test is slow (allocates a lot of memory)
    d = AtomicDict({FixedHash(k): None for k in range((1 << 25) - 1)})
    assert len(d.debug()["index"]) == 1 << 25
    d = AtomicDict(min_size=1 << 25)
    for k in range((1 << 25) - 2):
        d[FixedHash(k)] = None
    assert len(d.debug()["index"]) == 1 << 26


def test_dealloc():
    d = AtomicDict({"spam": 42})
    del d
    previous = None
    while (this := gc.collect()) != previous:
        previous = this


def test_concurrent_insert():
    d = AtomicDict(min_size=64 * 2)

    def thread_1():
        d[FixedHash(0)] = 1
        d[FixedHash(1)] = 1
        d[FixedHash(2)] = 1

    def thread_2():
        d[FixedHash(3)] = 2
        d[FixedHash(4)] = 2
        d[FixedHash(2)] = 2

    t1 = threading.Thread(target=thread_1)
    t2 = threading.Thread(target=thread_2)

    t1.start()
    t2.start()
    t1.join()
    t2.join()

    assert d[FixedHash(0)] == 1
    assert d[FixedHash(1)] == 1

    assert d[FixedHash(3)] == 2
    assert d[FixedHash(4)] == 2

    assert d[FixedHash(2)] in (1, 2)


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

    d = AtomicDict({FixedHash(_): None for _ in range(15)})
    assert d.debug()["blocks"][0]["entries"][14]  # exists
    del d[FixedHash(14)]
    with raises(KeyError):
        d[FixedHash(14)]
    debug = d.debug()
    assert debug["index"][14] == 0
    with raises(IndexError):
        debug["blocks"][0]["entries"][14]

    d = AtomicDict({FixedHash(_): None for _ in range(16)})
    del d[FixedHash(14)]
    with raises(KeyError):
        d[FixedHash(14)]
    debug = d.debug()
    assert debug["index"][14] == 0

    d = AtomicDict({FixedHash(_): None for _ in range(15)})
    d[FixedHash(64 + 14)] = None
    del d[FixedHash(14)]
    with raises(KeyError):
        d[FixedHash(14)]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({FixedHash(_): None for _ in range(15)})
    del d[FixedHash(7)]
    with raises(KeyError):
        d[FixedHash(7)]
    debug = d.debug()
    assert debug["index"][7] == 0

    d = AtomicDict({FixedHash(_): None for _ in range(8)})
    for _ in range(63 + 8, 63 + 8 + 7):
        d[FixedHash(_)] = None
    debug = d.debug()
    assert debug["index"][14] != 0
    del d[FixedHash(7)]
    with raises(KeyError):
        d[FixedHash(7)]
    debug = d.debug()
    assert debug["index"][14] == 0

    d = AtomicDict({FixedHash(_): None for _ in range(16)})
    del d[FixedHash(15)]
    with raises(KeyError):
        d[FixedHash(15)]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({FixedHash(_): None for _ in range(17)})
    del d[FixedHash(15)]
    with raises(KeyError):
        d[FixedHash(15)]
    debug = d.debug()
    assert debug["index"][15] == debug["meta"]["tombstone"]

    d = AtomicDict({FixedHash(16): None})
    del d[FixedHash(16)]
    with raises(KeyError):
        d[FixedHash(16)]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({FixedHash(16): None, FixedHash(17): None})
    del d[FixedHash(16)]
    with raises(KeyError):
        d[FixedHash(16)]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({FixedHash(15): None, FixedHash(16): None})
    del d[FixedHash(16)]
    with raises(KeyError):
        d[FixedHash(16)]
    debug = d.debug()
    assert debug["index"][16] == 0

    d = AtomicDict({FixedHash(15): None, FixedHash(16): None, FixedHash(17): None})
    del d[FixedHash(16)]
    with raises(KeyError):
        d[FixedHash(16)]
    debug = d.debug()
    assert debug["index"][16] == 0


def test_delete_with_swap():
    d = AtomicDict({FixedHash(_): None for _ in range(64)} | {FixedHash(_): None for _ in range(64, 128)})
    del d[FixedHash(64)]
    with raises(KeyError):
        del d[FixedHash(64)]
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
    d = AtomicDict({FixedHash(0): None, FixedHash(1): None, FixedHash(64): None, FixedHash(65): None})
    assert d.debug()["meta"]["log_size"] == 6
    assert len(list(filter(lambda _: _ != 0, Counter(d.debug()["index"]).keys()))) == len({0, 1, 64, 65})
    d[FixedHash(128)] = None
    assert d.debug()["meta"]["log_size"] == 7
    nodes = Counter(d.debug()["index"])
    assert len(list(filter(lambda _: _ != 0, nodes.keys()))) == len({0, 1, 64, 65, 128})
    for _ in nodes:
        if _ != 0:
            assert nodes[_] == 1
    for _ in [0, 1, 64, 65, 128]:
        assert d[FixedHash(_)] is None

    d = AtomicDict({FixedHash(_): None for _ in range(63)})
    assert d.debug()["meta"]["log_size"] == 7
    d[FixedHash(128)] = None
    assert d.debug()["meta"]["log_size"] == 7
    for _ in [*range(63), 128]:
        assert d[FixedHash(_)] is None


def test_compact():
    d = AtomicDict({FixedHash(0): None, FixedHash(1): None, FixedHash(64): None, FixedHash(65): None})
    for _ in range(20):
        d[FixedHash(2**_)] = None
        assert len(list(filter(lambda _: _ != 0, Counter(d.debug()["index"]).keys()))) == len(
            {0, 1, 64, 65, *[2**i for i in range(_ + 1)]}
        ), _
    for _ in [0, 1, 64, 65]:
        assert d[FixedHash(_)] is None
    for _ in range(20):
        assert d[FixedHash(2**_)] is None
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

    d = AtomicDict({FixedHash(0): None, FixedHash(1): None})
    for _ in range(20):
        d[FixedHash(2**_)] = None
    assert d.debug()["meta"]["log_size"] == 7
    d.compact()
    for _ in [0, *range(20)]:
        assert d[FixedHash(2**_)] is None
    # assert d.debug()["meta"]["log_size"] == 9
    assert d.debug()["meta"]["log_size"] == 19

    d = AtomicDict({}, min_size=2**16)
    assert len(d.debug()["index"]) == 2**16
    d.compact()
    assert len(d.debug()["index"]) == 2**16


def test_grow_then_shrink():
    d = AtomicDict()
    assert d.debug()["meta"]["log_size"] == 6

    for _ in range(2**10):
        debug = d.debug()
        assert len(Counter(debug["index"]).keys()) == _ + 1, debug["meta"]["log_size"]
        d[FixedHash(_)] = None
    assert d.debug()["meta"]["log_size"] == 11

    for _ in range(2**10):
        del d[FixedHash(_)]
    assert d.debug()["meta"]["log_size"] == 7  # cannot shrink back to 6

    debug = d.debug()
    empty = 0
    tombstone = 448
    assert len(Counter(debug["index"]).keys()) == len({empty, tombstone}), debug["meta"]["log_size"]

    for _ in range(2**20, 2**20 + 2**14):
        d[FixedHash(_)] = None

    assert d.debug()["meta"]["log_size"] == 15
    assert len(Counter(d.debug()["index"]).keys()) == 2**14 + 1

    for _ in range(2**20, 2**20 + 2**14):
        del d[FixedHash(_)]
    assert d.debug()["meta"]["log_size"] == 7


@pytest.mark.skip()
def test_large_grow_then_shrink():
    d = AtomicDict()
    assert d.debug()["meta"]["log_size"] == 6

    for _ in range(2**25):
        d[FixedHash(_)] = None
    assert d.debug()["meta"]["log_size"] == 26

    for _ in range(2**25):
        del d[FixedHash(_)]
    assert d.debug()["meta"]["log_size"] == 7


@pytest.mark.skip()
def test_large_pre_allocated_grow_then_shrink():
    d = AtomicDict(min_size=2**26)
    assert d.debug()["meta"]["log_size"] == 26

    for _ in range(2**25):
        d[FixedHash(_)] = None
    assert d.debug()["meta"]["log_size"] == 26

    for _ in range(2**25):
        del d[FixedHash(_)]
    assert d.debug()["meta"]["log_size"] == 26


def test_dont_implode():
    d = AtomicDict({1: None, 10: None})
    del d[1]
    d[2] = None
    assert d[2] is None
    assert d.debug()["meta"]["log_size"] == 6


def test_len_bounds():
    d = AtomicDict()

    assert d.len_bounds() == (0, 0)
    assert d.approx_len() == 0

    for _ in range(10):
        d[FixedHash(_)] = None

    assert d.len_bounds() == (10, 10)
    assert d.approx_len() == 10

    for _ in range(100):
        d[FixedHash(_)] = None

    assert d.len_bounds() == (100, 100)
    assert d.approx_len() == 100

    for _ in range(100):
        del d[FixedHash(_)]

    assert d.len_bounds() == (0, 0)
    assert d.approx_len() == 0


def test_fast_iter():
    d = AtomicDict(min_size=2 * 4 * 64 * 2)  # = 1024

    for p in range(4):
        for _ in range(p * 128, p * 128 + 64):
            d[FixedHash(_)] = 1
        for _ in range(p * 128, p * 128 + 64):
            d[FixedHash(_ + 64)] = 2

    def partition_1():
        n = 0
        for _, v in d.fast_iter(partitions=2, this_partition=0):
            assert v == 1
            n += 1
        assert n == 4 * 64

    def partition_2():
        n = 0
        for _, v in d.fast_iter(partitions=2, this_partition=1):
            assert v == 2
            n += 1
        assert n == 4 * 64

    threads = [
        threading.Thread(target=partition_1),
        threading.Thread(target=partition_2),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def test_compare_and_set():
    d = AtomicDict(
        {
            "spam": 0,
            "foo": None,
        }
    )
    d.compare_and_set(key="spam", expected=0, desired=100)
    assert d["spam"] == 100

    with pytest.raises(cereggii.ExpectationFailed):
        d.compare_and_set(key="foo", expected=100, desired=0)
    assert d["foo"] is None

    d.compare_and_set(key="witch", expected=cereggii.NOT_FOUND, desired="duck")
    assert d["witch"] == "duck"

    d.compare_and_set(key="foo", expected=cereggii.ANY, desired=0)
    assert d["foo"] == 0

    d.compare_and_set(key="bar", expected=cereggii.ANY, desired="baz")
    assert d["bar"] == "baz"


def test_batch_getitem():
    keys_count = 2**12
    d = AtomicDict({FixedHash(_): _ for _ in range(keys_count)})
    batch = {FixedHash(random.randrange(0, 2 * keys_count)): None for _ in range(keys_count // 2)}  # noqa: S311
    d.batch_getitem(batch)

    for k in batch:
        assert d.get(k, cereggii.NOT_FOUND) == batch[k]


def test_len():
    d = AtomicDict({FixedHash(_): None for _ in range(10)})
    assert len(d) == 10

    for _ in range(32):
        d[FixedHash(_ + 10)] = None

    assert len(d) == 42

    for _ in range(32):
        d[FixedHash(_ + 10)] = None

    assert len(d) == 42  # updates don't change count

    for _ in range(32):
        del d[FixedHash(_ + 10)]

    assert len(d) == 10
    assert len(d) == 10  # test twice for len_dirty
