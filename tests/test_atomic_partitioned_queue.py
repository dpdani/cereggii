# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0
import time
from threading import Barrier, Thread

import pytest
from cereggii import AtomicPartitionedQueue


def test_init():
    queue = AtomicPartitionedQueue()
    assert queue
    assert isinstance(queue, AtomicPartitionedQueue)
    assert queue.num_partitions == 1

    queue = AtomicPartitionedQueue(num_partitions=1)
    assert queue
    assert isinstance(queue, AtomicPartitionedQueue)
    assert queue.num_partitions == 1

    with pytest.raises(ValueError):
        AtomicPartitionedQueue(num_partitions=0)

    with pytest.raises(ValueError):
        AtomicPartitionedQueue(num_partitions=-1)


def test_producer():
    queue = AtomicPartitionedQueue()
    with queue.producer() as producer:
        assert producer
        # assert balance

    producer = queue.producer()
    with pytest.raises(RuntimeError):
        producer.put(None)


def test_consumer():
    queue = AtomicPartitionedQueue()
    with queue.consumer() as consumer:
        assert consumer
        # assert balance

    consumer = queue.consumer()
    with pytest.raises(RuntimeError):
        consumer.get()


# def test_producer_consumer():
#     queue = AtomicPartitionedQueue()
#
#     with queue.producer():
#         with pytest.raises(RuntimeError):
#             with queue.consumer():
#                 pass
#
#     with queue.consumer():
#         with pytest.raises(RuntimeError):
#             with queue.producer():
#                 pass


def test_put_then_get():
    queue = AtomicPartitionedQueue()
    with queue.producer() as producer:
        for i in range(4096):
            producer.put(i)
    with queue.consumer() as consumer:
        for i in range(4096):
            assert consumer.get() == i


def test_consumer_producer_threads():
    queue = AtomicPartitionedQueue()

    def producer():
        with queue.producer() as producer:
            for i in range(4096):
                producer.put(i)

    def consumer():
        with queue.consumer() as consumer:
            for i in range(4096):
                assert consumer.get() == i

    threads = [
        Thread(target=producer),
        Thread(target=consumer),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

def test_consumer_waits_for_producer():
    queue = AtomicPartitionedQueue()
    barrier = Barrier(2)

    def producer():
        barrier.wait()
        time.sleep(.1)
        with queue.producer() as producer:
            for i in range(4096):
                producer.put(i)

    def consumer():
        barrier.wait()
        with queue.consumer() as consumer:
            for i in range(4096):
                assert consumer.get() == i

    threads = [
        Thread(target=producer),
        Thread(target=consumer),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
