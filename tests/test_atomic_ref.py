# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import gc
import threading

import cereggii
from cereggii import AtomicRef


def test_init():
    r = AtomicRef()
    assert r.get() is None
    obj = object()
    id_obj = id(obj)
    r = AtomicRef(obj)
    assert r.get() is obj
    assert id(obj) == id_obj
    r = AtomicRef(initial_value=obj)
    assert r.get() is obj
    assert id(obj) == id_obj
    AtomicRef[bool](True)


def test_set():
    obj = object()
    id_obj = id(obj)
    r = AtomicRef()
    r.set(obj)
    assert r.get() is obj
    assert id(obj) == id_obj


class Result:
    def __init__(self):
        self.result: int | object | None = 0


def test_cas():
    r = AtomicRef()
    result_0 = Result()
    result_1 = Result()

    def thread(result, ref):
        result.result = r.compare_and_set(None, ref)

    obj_0 = object()
    id_0 = id(obj_0)
    t0 = threading.Thread(target=thread, args=(result_0, obj_0))
    obj_1 = object()
    id_1 = id(obj_1)
    t1 = threading.Thread(target=thread, args=(result_1, obj_1))
    t0.start()
    t1.start()
    t0.join()
    t1.join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_counter():
    r = AtomicRef(0)

    def thread():
        for _ in range(1_000):
            expected = r.get()
            while not r.compare_and_set(expected, expected + 1):
                expected = r.get()

    t0 = threading.Thread(target=thread)
    t1 = threading.Thread(target=thread)
    t0.start()
    t1.start()
    t0.join()
    t1.join()

    assert r.get() == 2_000


def test_swap():
    r = AtomicRef()
    result_0 = Result()
    result_1 = Result()

    def thread(result, ref):
        result.result = r.get_and_set(ref)

    obj_0 = object()
    id_0 = id(obj_0)
    obj_1 = object()
    id_1 = id(obj_1)
    t0 = threading.Thread(
        target=thread,
        args=(
            result_0,
            obj_0,
        ),
    )
    t1 = threading.Thread(
        target=thread,
        args=(
            result_1,
            obj_1,
        ),
    )
    t0.start()
    t1.start()
    t0.join()
    t1.join()
    results = [result_0.result, result_1.result]

    assert None in results
    results.remove(None)
    assert results[0] in [obj_0, obj_1]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_dealloc():
    obj = object()
    id_obj = id(obj)
    r = AtomicRef(obj)
    assert r.get() is obj
    del r
    previous = None
    while (this := gc.collect()) != previous:
        previous = this
    assert id(obj) == id_obj


def test_get_handle():
    d = AtomicRef()
    h = d.get_handle()
    assert isinstance(h, cereggii.ThreadHandle)
    assert isinstance(h.get_handle(), cereggii.ThreadHandle)
