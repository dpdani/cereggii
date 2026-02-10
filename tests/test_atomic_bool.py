# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from cereggii import AtomicBool, ExpectationFailed
from threading import Barrier

from .utils import TestingThreadSet


def test_init():
    b = AtomicBool()
    assert b.get() is False
    b = AtomicBool(True)
    assert b.get() is True
    b = AtomicBool(False)
    assert b.get() is False
    b = AtomicBool(initial_value=True)
    assert b.get() is True


def test_set():
    b = AtomicBool()
    assert b.get() is False
    b.set(True)
    assert b.get() is True
    b.set(False)
    assert b.get() is False


def test_set_type_error():
    b = AtomicBool()
    with pytest.raises(TypeError, match="must be a bool"):
        b.set(1)
    with pytest.raises(TypeError, match="must be a bool"):
        b.set(None)
    with pytest.raises(TypeError, match="must be a bool"):
        b.set("true")


class Result:
    def __init__(self):
        self.result: bool | None = None


def test_cas():
    b = AtomicBool(False)
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result, desired):
        result.result = b.compare_and_set(False, desired)

    TestingThreadSet(
        thread(result_0, True),
        thread(result_1, True),
    ).start_and_join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert b.get() is True


def test_cas_type_error():
    b = AtomicBool()
    with pytest.raises(TypeError, match="expected=.* must be a bool"):
        b.compare_and_set(1, True)
    with pytest.raises(TypeError, match="expected=.* must be a bool"):
        b.compare_and_set(None, True)
    with pytest.raises(TypeError, match="desired=.* must be a bool"):
        b.compare_and_set(False, 1)
    with pytest.raises(TypeError, match="desired=.* must be a bool"):
        b.compare_and_set(False, None)


def test_toggle():
    b = AtomicBool(False)
    num_threads = 2
    barrier = Barrier(num_threads)

    @TestingThreadSet.repeat(num_threads)
    def threads():
        barrier.wait()
        for _ in range(100):
            expected = b.get()
            while not b.compare_and_set(expected, not expected):
                expected = b.get()

    threads.start_and_join()
    assert b.get() is False


def test_swap():
    b = AtomicBool(False)
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result, desired):
        result.result = b.get_and_set(desired)

    TestingThreadSet(
        thread(result_0, True),
        thread(result_1, True),
    ).start_and_join()
    results = [result_0.result, result_1.result]

    assert False in results
    results.remove(False)
    assert results[0] is True
    assert b.get() is True


def test_get_and_set_type_error():
    b = AtomicBool()
    with pytest.raises(TypeError, match="must be a bool"):
        b.get_and_set(1)
    with pytest.raises(TypeError, match="must be a bool"):
        b.get_and_set(None)
    with pytest.raises(TypeError, match="must be a bool"):
        b.get_and_set("true")


def test_get_handle():
    b = AtomicBool(True)
    h = b.get_handle()
    assert isinstance(h, AtomicBool)
    assert h.get() is True
    assert isinstance(h.get_handle(), AtomicBool)


def test_set_or_raise_success():
    b = AtomicBool(False)
    b.set_or_raise()
    assert b.get() is True


def test_set_or_raise_already_set():
    b = AtomicBool(True)
    with pytest.raises(ExpectationFailed, match="already set to True"):
        b.set_or_raise()
    assert b.get() is True


def test_set_or_raise_concurrent():
    b = AtomicBool(False)
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result):
        try:
            b.set_or_raise()
            result.result = True
        except ExpectationFailed:
            result.result = False

    TestingThreadSet(
        thread(result_0),
        thread(result_1),
    ).start_and_join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert b.get() is True


def test_reset_or_raise_success():
    b = AtomicBool(True)
    b.reset_or_raise()
    assert b.get() is False


def test_reset_or_raise_already_reset():
    b = AtomicBool(False)
    with pytest.raises(ExpectationFailed, match="already set to False"):
        b.reset_or_raise()
    assert b.get() is False


def test_reset_or_raise_concurrent():
    b = AtomicBool(True)
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result):
        try:
            b.reset_or_raise()
            result.result = True
        except ExpectationFailed:
            result.result = False

    TestingThreadSet(
        thread(result_0),
        thread(result_1),
    ).start_and_join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert b.get() is False
