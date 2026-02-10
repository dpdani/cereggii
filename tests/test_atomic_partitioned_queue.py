# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from cereggii import AtomicPartitionedQueue, AtomicInt64
from threading import Barrier

from .utils import TestingThreadSet


def test_init():
    q = AtomicPartitionedQueue()
    assert isinstance(q, AtomicPartitionedQueue)


def test_put():
    q = AtomicPartitionedQueue()
    q.put("item1")
    q.put("item2")
    q.put(123)


def test_get():
    q = AtomicPartitionedQueue()
    q.put("item")
    result = q.get()
    assert result == "item"


def test_get_with_block():
    q = AtomicPartitionedQueue()
    q.put("item")
    result = q.get(block=True)
    assert result == "item"


def test_get_with_timeout():
    q = AtomicPartitionedQueue()
    q.put("item")
    result = q.get(block=True, timeout=1.0)
    assert result == "item"


def test_get_non_blocking():
    q = AtomicPartitionedQueue()
    q.put("item")
    result = q.get(block=False)
    assert result == "item"


def test_try_get():
    q = AtomicPartitionedQueue()
    q.put("item")
    result = q.try_get()
    assert result == "item"


def test_try_get_empty():
    q = AtomicPartitionedQueue()
    result = q.try_get()
    assert result is None


def test_approx_len():
    q = AtomicPartitionedQueue()
    length = q.approx_len()
    assert isinstance(length, int)
    assert length >= 0


def test_approx_len_after_put():
    q = AtomicPartitionedQueue()
    assert q.approx_len() == 0
    q.put("item1")
    q.put("item2")
    assert q.approx_len() == 2


def test_close():
    q = AtomicPartitionedQueue()
    q.close()


def test_closed_property():
    q = AtomicPartitionedQueue()
    assert not q.closed
    q.close()
    assert q.closed


def test_close_then_put():
    q = AtomicPartitionedQueue()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        q.put("item")


def test_close_then_get():
    q = AtomicPartitionedQueue()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        q.get()


def test_multiple_put_get():
    q = AtomicPartitionedQueue()

    items = ["a", "b", "c", 1, 2, 3, None, True, False]
    for item in items:
        q.put(item)

    for item in items:
        result = q.try_get()
        assert result == item


def test_put_various_types():
    q = AtomicPartitionedQueue()

    # Test different types
    q.put(42)
    q.put("string")
    q.put([1, 2, 3])
    q.put({"key": "value"})
    q.put(object())
    q.put(None)

    assert q.approx_len() == 6

def test_producers_consumers():
    q = AtomicPartitionedQueue()
    num_producers = 10
    num_consumers = 10
    barrier = Barrier(num_producers + num_consumers)
    items_per_producer = 10
    dequeued = [AtomicInt64(0) for _ in range(items_per_producer * num_producers)]

    @TestingThreadSet.range(num_producers)
    def producers(me):
        barrier.wait()
        for i in range(items_per_producer):
            q.put(me * num_producers + i)

    @TestingThreadSet.repeat(num_consumers)
    def consumers():
        barrier.wait()
        for _ in range(items_per_producer * 2):
            item = q.try_get()
            if item is not None:
                dequeued[item].increment_and_get(1)

    (producers | consumers).start_and_join()

    # consumer leftovers (consumers may have finished before producers)
    while (item := q.try_get()) is not None:
        dequeued[item].increment_and_get(1)

    for item in dequeued:
        assert item.get() == 1


def test_concurrent_put_blocking_get():
    q = AtomicPartitionedQueue()
    num_items = 200
    num_producers = 1
    num_consumers = 2
    barrier = Barrier(num_producers + num_consumers)
    consumed = AtomicInt64(0)

    @TestingThreadSet.repeat(num_producers)
    def producers():
        barrier.wait()
        for i in range(num_items):
            q.put(f"item_{i}")

    @TestingThreadSet.repeat(num_consumers)
    def consumers():
        barrier.wait()
        for _ in range(num_items // num_consumers):
            q.get(block=True, timeout=5.0)
            consumed.increment_and_get(1)

    (producers | consumers).start_and_join()

    assert consumed.get() == num_items


def test_concurrent_approx_len():
    q = AtomicPartitionedQueue()
    num_producers = 2
    num_consumers = 2
    num_len_checkers = 2
    barrier = Barrier(num_producers + num_consumers + num_len_checkers)
    repetitions = 100

    @TestingThreadSet.repeat(num_producers)
    def producer():
        barrier.wait()
        for i in range(repetitions):
            q.put(i)

    @TestingThreadSet.repeat(num_consumers)
    def consumer():
        barrier.wait()
        for _ in range(repetitions):
            q.try_get()

    @TestingThreadSet.repeat(num_len_checkers)
    def length_checker():
        barrier.wait()
        for _ in range(repetitions):
            assert 0 <= q.approx_len() <= repetitions * num_producers

    (producer | consumer | length_checker).start_and_join()



def test_blocking_get_timeout_behavior():
    q = AtomicPartitionedQueue()
    timeouts = []

    @TestingThreadSet.repeat(4)
    def consumers():
        import time
        start = time.time()
        try:
            q.get(block=True, timeout=0.1)
        except TimeoutError:
            pass
        elapsed = time.time() - start
        timeouts.append(elapsed)

    consumers.start_and_join()

    assert timeouts
    for elapsed in timeouts:
        assert 0.1 <= elapsed <= 0.2  # allow some timing variance
