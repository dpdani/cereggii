import threading
import time

from cereggii import AtomicDict, NOT_FOUND


size = 3628800
expected_total = size * 5
print(f"{expected_total=}")


def make_keys():
    return list(range(10))


def make_data(data_size):
    keys = make_keys()
    return [(keys[_ % len(keys)], 5) for _ in range(data_size)]


def builtin_dict_sum():
    d = {}
    keys = make_keys()
    for k in keys:
        d[k] = 0
    for k, v in make_data(size):
        d[k] += v
    return sum(d.values())


def thread(atomic_dict, iterations):
    data = make_data(iterations)

    def my_reduce_sum(key, current, new):
        if current is NOT_FOUND:
            return new
        return current + new

    atomic_dict.reduce(data, my_reduce_sum)


def thread_specialized(atomic_dict, iterations):
    data = make_data(iterations)
    atomic_dict.reduce_sum(data)


def threaded_sum(threads_num, thread_target):
    atomic_dict = AtomicDict()
    data_size = size // threads_num

    threads = [
        threading.Thread(target=thread_target, args=(atomic_dict, data_size))
        for _ in range(threads_num)
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    threaded_total = 0
    for _, v in atomic_dict.fast_iter():
        threaded_total += v
    return threaded_total


def time_and_run(func, *args):
    took = []
    repeats = 3
    for _ in range(repeats):
        started = time.perf_counter()
        ret = func(*args)
        _took = time.perf_counter() - started
        took.append(_took)
    average = sum(took) / repeats
    return average, ret


print("\nSummation using the built-in dict with a single thread:")
took_dict, total = time_and_run(builtin_dict_sum)
print(f" - Took {took_dict:.3f}s ({total=})")

thread_counts = [1, 2, 3, 4]
print("\nSummation using cereggii.AtomicDict.reduce():")
for count in thread_counts:
    took, total = time_and_run(threaded_sum, count, thread)
    print(f" - Took {took:.3f}s with {count} threads ({took_dict / took:.1f}x faster, {total=})")

print("\nSummation using cereggii.AtomicDict.reduce_sum():")
for count in thread_counts:
    took, total = time_and_run(threaded_sum, count, thread_specialized)
    print(f" - Took {took:.3f}s with {count} threads ({took_dict / took:.1f}x faster, {total=})")
