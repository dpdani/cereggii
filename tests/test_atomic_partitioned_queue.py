# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from cereggii import AtomicPartitionedQueue, AtomicInt64, AtomicBool
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


def test_producer_context_manager():
    q = AtomicPartitionedQueue()
    with q.producer() as p:
        p.put("item1")
        p.put("item2")
    assert q.try_get() == "item1"
    assert q.try_get() == "item2"


def test_producer_without_context_manager():
    q = AtomicPartitionedQueue()
    p = q.producer()
    p.put("a")
    assert q.try_get() == "a"


def test_consumer_context_manager():
    q = AtomicPartitionedQueue()
    q.put("item1")
    q.put("item2")
    with q.consumer() as c:
        assert c.get() == "item1"
        assert c.try_get() == "item2"


def test_consumer_try_get_empty():
    q = AtomicPartitionedQueue()
    with q.consumer() as c:
        assert c.try_get() is None


def test_producer_consumer_roundtrip():
    q = AtomicPartitionedQueue()
    with q.producer() as p, q.consumer() as c:
        p.put(None)
        p.put(42)
        p.put("hello")
        assert c.get() is None
        assert c.get() == 42
        assert c.get() == "hello"


def test_producer_put_after_close():
    q = AtomicPartitionedQueue()
    p = q.producer()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        p.put("item")


def test_consumer_get_after_close():
    q = AtomicPartitionedQueue()
    c = q.consumer()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        c.get()


def test_producer_keeps_queue_alive():
    q = AtomicPartitionedQueue()
    p = q.producer()
    del q
    p.put("survived")
    # producer still holds a reference to the queue's impl


def test_concurrent_producers_consumers_with_tokens():
    q = AtomicPartitionedQueue()
    num_producers = 8
    num_consumers = 8
    barrier = Barrier(num_producers + num_consumers)
    items_per_producer = 50
    dequeued = [AtomicInt64(0) for _ in range(items_per_producer * num_producers)]

    @TestingThreadSet.range(num_producers)
    def producers(me):
        with q.producer() as p:
            barrier.wait()
            for i in range(items_per_producer):
                p.put(me * items_per_producer + i)

    @TestingThreadSet.repeat(num_consumers)
    def consumers():
        with q.consumer() as c:
            barrier.wait()
            for _ in range(items_per_producer * num_producers // num_consumers):
                item = c.try_get()
                if item is not None:
                    dequeued[item].increment_and_get(1)

    (producers | consumers).start_and_join()

    # consumer leftovers
    while (item := q.try_get()) is not None:
        dequeued[item].increment_and_get(1)

    for item in dequeued:
        assert item.get() == 1


def test_put_many():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2, 3, 4, 5])
    assert q.approx_len() == 5
    for expected in [1, 2, 3, 4, 5]:
        assert q.try_get() == expected


def test_put_many_empty():
    q = AtomicPartitionedQueue()
    q.put_many([])
    assert q.approx_len() == 0


def test_put_many_iterable():
    q = AtomicPartitionedQueue()
    q.put_many(range(5))
    assert q.approx_len() == 5
    for expected in range(5):
        assert q.try_get() == expected


def test_put_many_generator():
    q = AtomicPartitionedQueue()
    q.put_many(i * 2 for i in range(4))
    for expected in [0, 2, 4, 6]:
        assert q.try_get() == expected


def test_put_many_various_types():
    q = AtomicPartitionedQueue()
    items = ["a", 1, None, True, [1, 2], {"k": "v"}]
    q.put_many(items)
    for expected in items:
        assert q.try_get() == expected


def test_put_many_after_close():
    q = AtomicPartitionedQueue()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        q.put_many([1, 2, 3])


def test_put_many_non_iterable():
    q = AtomicPartitionedQueue()
    with pytest.raises(TypeError):
        q.put_many(42)


def test_get_many():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2, 3, 4, 5])
    result = q.get_many(5)
    assert result == [1, 2, 3, 4, 5]


def test_get_many_partial():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2, 3])
    result = q.get_many(10, block=False)
    assert result == [1, 2, 3]


def test_get_many_more_than_available_blocking_timeout():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2])
    # blocking with timeout returns whatever's available once at least one item is there
    result = q.get_many(10, block=True, timeout=0.5)
    assert 1 <= len(result) <= 2
    assert all(x in [1, 2] for x in result)


def test_get_many_negative_max_items():
    q = AtomicPartitionedQueue()
    with pytest.raises(ValueError, match="max_items must be >= 1"):
        q.get_many(-1)
    with pytest.raises(ValueError, match="max_items must be >= 1"):
        q.get_many(0)


def test_get_many_non_blocking_empty():
    q = AtomicPartitionedQueue()
    assert q.get_many(5, block=False) == []


def test_get_many_timeout_empty():
    q = AtomicPartitionedQueue()
    result = q.get_many(5, block=True, timeout=0.1)
    assert result == []


def test_get_many_after_close():
    q = AtomicPartitionedQueue()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        q.get_many(5)


def test_try_get_many():
    q = AtomicPartitionedQueue()
    q.put_many(["a", "b", "c"])
    result = q.try_get_many(5)
    assert result == ["a", "b", "c"]


def test_try_get_many_empty():
    q = AtomicPartitionedQueue()
    assert q.try_get_many(5) == []


def test_try_get_many_negative():
    q = AtomicPartitionedQueue()
    with pytest.raises(ValueError, match="max_items must be >= 1"):
        q.try_get_many(-1)
    with pytest.raises(ValueError, match="max_items must be >= 1"):
        q.try_get_many(0)


def test_get_many_keyword_args():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2, 3])
    result = q.get_many(max_items=10, block=False)
    assert result == [1, 2, 3]


def test_producer_put_many():
    q = AtomicPartitionedQueue()
    with q.producer() as p:
        p.put_many([10, 20, 30])
    assert q.try_get() == 10
    assert q.try_get() == 20
    assert q.try_get() == 30


def test_consumer_get_many():
    q = AtomicPartitionedQueue()
    q.put_many([1, 2, 3, 4])
    with q.consumer() as c:
        result = c.get_many(4)
    assert result == [1, 2, 3, 4]


def test_consumer_try_get_many_empty():
    q = AtomicPartitionedQueue()
    with q.consumer() as c:
        assert c.try_get_many(5) == []


def test_consumer_try_get_many():
    q = AtomicPartitionedQueue()
    q.put_many(["x", "y", "z"])
    with q.consumer() as c:
        result = c.try_get_many(10)
    assert result == ["x", "y", "z"]


def test_consumer_get_many_after_close():
    q = AtomicPartitionedQueue()
    c = q.consumer()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        c.get_many(5)


def test_producer_put_many_after_close():
    q = AtomicPartitionedQueue()
    p = q.producer()
    q.close()
    with pytest.raises(RuntimeError, match="queue is closed"):
        p.put_many([1, 2])


# @pytest.mark.skip
def test_concurrent_put_many_get_many():
    q = AtomicPartitionedQueue()
    num_producers = 4
    num_consumers = 4
    batch_size = 25
    batches_per_producer = 4
    items_per_producer = batch_size * batches_per_producer
    total_items = items_per_producer * num_producers
    barrier = Barrier(num_producers + num_consumers)
    dequeued = [AtomicBool(False) for _ in range(total_items)]
    assert total_items % num_consumers == 0

    @TestingThreadSet.range(num_producers)
    def producers(me):
        with q.producer() as p:
            barrier.wait()
            base = me * items_per_producer
            for batch in range(batches_per_producer):
                items = [base + batch * batch_size + i for i in range(batch_size)]
                p.put_many(items)

    @TestingThreadSet.repeat(num_consumers)
    def consumers():
        collected = 0
        target = total_items // num_consumers
        with q.consumer() as c:
            barrier.wait()
            while collected < target:
                items = c.get_many(min(batch_size, target - collected), timeout=0.1)
                for item in items:
                    dequeued[item].set_or_raise()
                    collected += 1

    (producers | consumers).start_and_join()

    assert all(i.get() for i in dequeued)


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
