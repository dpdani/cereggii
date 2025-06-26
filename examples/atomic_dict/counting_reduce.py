import gc
import threading
import time

from cereggii import AtomicDict, NOT_FOUND


use_optimized_sum = False
print(f"{use_optimized_sum=}")

expected_total = 3628800
print(f"{expected_total=}")


def reduce_example(threads_num):
    atomic_dict = AtomicDict()
    for k in range(10):
        atomic_dict[k] = 0
    iterations = expected_total // threads_num

    def incr():
        keys = list(range(10))
        data = [(keys[_ % len(keys)], 1) for _ in range(iterations)]

        def reduce_sum(key, current, new):
            if current is NOT_FOUND:
                return new
            return current + new

        if use_optimized_sum:
            atomic_dict.reduce(data, sum)
        else:
            atomic_dict.reduce(data, reduce_sum)

    threads = [threading.Thread(target=incr) for _ in range(threads_num)]

    started = time.perf_counter()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    took = time.perf_counter() - started
    total = 0
    for _, v in atomic_dict.fast_iter():
        total += v
    print(f" - Took {took:.3f}s with {threads_num} threads ({took_dict / took:.1f}x faster, {total=})")


gc.disable()

print("\nCounting keys using the built-in dict with a single thread:")
start = time.perf_counter()
d = {}
keys = list(range(10))
for k in keys:
    d[k] = 0
for _ in range(expected_total):
    d[keys[_ % len(keys)]] += 1
took_dict = time.perf_counter() - start
total = sum(d.values())
print(f" - Took {took_dict:.3f}s ({total=})")

print("\nCounting keys using cereggii.AtomicDict.reduce():")
thread_counts = [1, 2, 3, 4]
for count in thread_counts:
    reduce_example(count)
gc.enable()
