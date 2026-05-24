"""Microbenchmark for AtomicPartitionedQueue.

Exercises the four main modes of operation with N producer threads and N
consumer threads each:

  1. plain   put / get
  2. ctx     put / get with producer/consumer contexts
  3. bulk    put_many / get_many
  4. bulk+ctx put_many / get_many with producer/consumer contexts

Each mode enqueues NUM_PRODUCERS * ITEMS_PER_PRODUCER items in total and
the consumers cooperatively drain them all. Reported numbers are wall-clock
time and total items per second (producer + consumer side combined).
"""

import collections
import queue
import threading
import time
from threading import Barrier

from cereggii import AtomicInt64, AtomicPartitionedQueue


NUM_PRODUCERS = 4
NUM_CONSUMERS = 4
ITEMS_PER_PRODUCER = 200_000
BATCH = 64

TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER
assert TOTAL_ITEMS % NUM_CONSUMERS == 0


def _run(label, producer_fn, consumer_fn, queue_factory=AtomicPartitionedQueue):
    q = queue_factory()
    barrier = Barrier(NUM_PRODUCERS + NUM_CONSUMERS + 1)
    consumed = AtomicInt64(0)

    producers = [threading.Thread(target=producer_fn, args=(q, barrier, i)) for i in range(NUM_PRODUCERS)]
    consumers = [threading.Thread(target=consumer_fn, args=(q, barrier, consumed)) for _ in range(NUM_CONSUMERS)]

    for t in producers + consumers:
        t.start()

    barrier.wait()
    started = time.perf_counter()
    for t in producers + consumers:
        t.join()
    elapsed = time.perf_counter() - started

    got = consumed.get()
    rate = got / elapsed if elapsed > 0 else float("inf")
    status = "" if got == TOTAL_ITEMS else f"[MISMATCH (got {got}/{TOTAL_ITEMS})]"
    print(f"  {label:<25} {elapsed:7.3f}s   {rate/1e6:7.2f} M items/s   {status}")


# 1. plain put / get
def _prod_plain(q, barrier, pid):
    base = pid * ITEMS_PER_PRODUCER
    barrier.wait()
    for i in range(ITEMS_PER_PRODUCER):
        q.put(base + i)


def _cons_plain(q, barrier, consumed):
    local_consumed = 0
    barrier.wait()
    while local_consumed < TOTAL_ITEMS // NUM_CONSUMERS:
        _ = q.get()
        local_consumed += 1
    consumed += local_consumed


# 2. put / get with contexts
def _prod_ctx(q, barrier, pid):
    base = pid * ITEMS_PER_PRODUCER
    with q.producer() as p:
        barrier.wait()
        for i in range(ITEMS_PER_PRODUCER):
            p.put(base + i)


def _cons_ctx(q, barrier, consumed):
    local_consumed = 0
    with q.consumer() as c:
        barrier.wait()
        while local_consumed < TOTAL_ITEMS // NUM_CONSUMERS:
            _ = c.get()
            local_consumed += 1
    consumed += local_consumed


# 3. put_many / get_many
def _prod_bulk(q, barrier, pid):
    base = pid * ITEMS_PER_PRODUCER
    barrier.wait()
    for start in range(0, ITEMS_PER_PRODUCER, BATCH):
        q.put_many(range(base + start, base + start + BATCH))


def _cons_bulk(q, barrier, consumed):
    local_consumed = 0
    target_consumed = TOTAL_ITEMS // NUM_CONSUMERS
    barrier.wait()
    while local_consumed < target_consumed:
        items = q.get_many(min(BATCH, target_consumed - local_consumed))
        if items:
            local_consumed += len(items)
    consumed += local_consumed


# 4. put_many / get_many with contexts
def _prod_bulk_ctx(q, barrier, pid):
    base = pid * ITEMS_PER_PRODUCER
    with q.producer() as p:
        barrier.wait()
        for start in range(0, ITEMS_PER_PRODUCER, BATCH):
            p.put_many(range(base + start, base + start + BATCH))


def _cons_bulk_ctx(q, barrier, consumed):
    local_consumed = 0
    target_consumed = TOTAL_ITEMS // NUM_CONSUMERS
    with q.consumer() as c:
        barrier.wait()
        while local_consumed < target_consumed:
            items = c.try_get_many(min(BATCH, target_consumed - local_consumed))
            if items:
                local_consumed += len(items)
    consumed += local_consumed


class WrappedDeque(collections.deque):
    put = collections.deque.append

    def get(self):
        try:
            return self.popleft()
        except IndexError:
            return None


def main():
    print(
        f"AtomicPartitionedQueue bench: "
        f"{NUM_PRODUCERS} producers x {NUM_CONSUMERS} consumers, "
        f"{TOTAL_ITEMS:,} total items, batch={BATCH}\n"
    )
    print(f"  {'mode':<25} {'time':>7}     {'throughput':>16}")
    print(f"  {'-'*25} {'-'*7}     {'-'*16}")

    _run("stdlib queue.Queue", _prod_plain, _cons_plain, queue_factory=queue.Queue)
    _run("stdlib deque", _prod_plain, _cons_plain, queue_factory=WrappedDeque)
    _run("plain put/get", _prod_plain, _cons_plain)
    _run("ctx put/get", _prod_ctx, _cons_ctx)
    _run("plain put_many/get_many", _prod_bulk, _cons_bulk)
    _run("ctx put_many/get_many", _prod_bulk_ctx, _cons_bulk_ctx)


if __name__ == "__main__":
    main()
