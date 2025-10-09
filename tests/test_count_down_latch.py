import threading
import time

import pytest
from cereggii import CountDownLatch

from .utils import TestingThreadSet


def test_init():
    latch = CountDownLatch(1)
    assert latch.get() == 1
    latch = CountDownLatch(5)
    assert latch.get() == 5
    latch = CountDownLatch(0)
    assert latch.get() == 0
    with pytest.raises(AssertionError, match="count must be >= 0"):
        CountDownLatch(-1)


def test_decrement_reduces_count():
    latch = CountDownLatch(3)
    latch.decrement()
    assert latch.get() == 2
    latch.decrement()
    assert latch.get() == 1
    latch.decrement()
    assert latch.get() == 0


def test_decrement_at_zero_does_nothing():
    latch = CountDownLatch(0)
    assert latch.get() == 0
    latch.decrement()
    assert latch.get() == 0


def test_decrement_and_get_returns_new_count():
    latch = CountDownLatch(3)
    result = latch.decrement_and_get()
    assert result == 2
    assert latch.get() == 2
    result = latch.decrement_and_get()
    assert result == 1
    assert latch.get() == 1


def test_decrement_and_get_at_zero_returns_zero():
    latch = CountDownLatch(0)
    result = latch.decrement_and_get()
    assert result == 0
    assert latch.get() == 0


def test_wait_with_zero_count_returns_immediately():
    latch = CountDownLatch(0)
    start_time = time.time()
    latch.wait()
    end_time = time.time()
    assert end_time - start_time < 0.1


def test_wait_blocks_until_count_reaches_zero():
    latch = CountDownLatch(1)
    wait_completed = threading.Event()
    barrier = threading.Barrier(2)

    @TestingThreadSet.repeat(1)
    def waiter():
        barrier.wait()
        latch.wait()
        wait_completed.set()

    waiter.start()
    barrier.wait()
    # Give some time to ensure wait() is blocking
    time.sleep(0.01)
    assert not wait_completed.is_set()
    # Decrement to release the wait
    latch.decrement()
    # Wait should complete now
    waiter.join(timeout=1.0)
    assert wait_completed.is_set()


def test_multiple_threads_waiting():
    latch = CountDownLatch(1)
    num_waiters = 5
    completed_waiters = []
    barrier = threading.Barrier(num_waiters + 1)

    @TestingThreadSet.range(num_waiters)
    def waiters(waiter_id):
        barrier.wait()
        latch.wait()
        completed_waiters.append(waiter_id)

    waiters.start()
    # Give time for all threads to start waiting
    barrier.wait(timeout=1.0)
    time.sleep(0.01)
    assert len(completed_waiters) == 0
    # Release all waiters
    latch.decrement()
    # Wait for all threads to complete
    waiters.join(timeout=1.0)
    assert len(completed_waiters) == num_waiters
    assert set(completed_waiters) == set(range(num_waiters))


def test_multiple_decrements_by_different_threads():
    initial_count = 10
    latch = CountDownLatch(initial_count)
    barrier = threading.Barrier(initial_count)

    @TestingThreadSet.repeat(initial_count)
    def decrementers():
        barrier.wait()
        latch.decrement()

    decrementers.start_and_join(join_timeout=1.0)
    assert latch.get() == 0


def test_repr_shows_current_count():
    latch = CountDownLatch(5)
    repr_str = repr(latch)

    assert "CountDownLatch" in repr_str
    assert "count=5" in repr_str
    assert hex(id(latch)) in repr_str


def test_repr_updates_with_count_changes():
    latch = CountDownLatch(2)
    initial_repr = repr(latch)
    assert "count=2" in initial_repr
    latch.decrement()
    updated_repr = repr(latch)
    assert "count=1" in updated_repr


def test_concurrent_decrement_and_wait():
    num_waiters = 2
    num_decrementers = 3
    latch = CountDownLatch(num_decrementers)
    barrier = threading.Barrier(num_waiters + num_decrementers)
    log = []

    @TestingThreadSet.range(num_waiters)
    def waiters(waiter_id):
        barrier.wait()
        latch.wait()
        log.append(f"waiter_{waiter_id}_completed")

    @TestingThreadSet.range(num_decrementers)
    def decrementers(decrementer_id):
        barrier.wait()
        result = latch.decrement_and_get()
        log.append(f"decrementer_{decrementer_id}_result_{result}")

    (waiters | decrementers).start_and_join(join_timeout=2.0)

    waiter_completions = [r for r in log if "waiter" in r]
    assert len(waiter_completions) == 2
    decrement_results = [r for r in log if "decrementer" in r]
    assert len(decrement_results) == 3


def test_many_operations():
    num_waiters = 10
    num_decrementers = 10
    decrements_per_thread = 5
    initial_count = num_decrementers * decrements_per_thread
    latch = CountDownLatch(initial_count)
    log = []
    barrier = threading.Barrier(num_waiters + num_decrementers)

    @TestingThreadSet.range(num_waiters)
    def waiters(waiter_id):
        barrier.wait()
        latch.wait()
        log.append(f"wait_{waiter_id}")

    @TestingThreadSet.range(num_decrementers)
    def decrementers(decrementer_id):
        barrier.wait()
        for i in range(decrements_per_thread):
            result = latch.decrement_and_get()
            log.append(f"dec_{decrementer_id}_{i}_{result}")

    (waiters | decrementers).start_and_join(join_timeout=5.0)

    assert latch.get() == 0
    wait_logs = [op for op in log if op.startswith("wait_")]
    assert len(wait_logs) == num_waiters
    decrement_logs = [r for r in log if "dec_" in r]
    assert len(decrement_logs) == initial_count
