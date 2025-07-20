import random
import threading
import time

from cereggii import AtomicDict, NOT_FOUND, ThreadHandle


size = 3628800
values = [1, 2, 3]
expected_avg = sum(values) / len(values)
print(f"{expected_avg=:.2f}")


def make_keys():
    return list(range(10))


def make_data(data_size):
    keys = make_keys()
    rand = random.Random()
    v = ThreadHandle(values)
    return [(keys[_ % len(keys)], rand.choice(v)) for _ in range(data_size)]


def builtin_dict_avg():
    d = {}
    keys = make_keys()
    for k in keys:
        d[k] = (0, 0)
    for k, v in make_data(size):
        tot, cnt = d[k]
        d[k] = (tot + v, cnt + 1)

    for k, (tot, cnt) in d.items():
        d[k] = tot / cnt

    total = 0
    count = 0
    for k, avg in d.items():
        total += avg
        count += 1
    return total / count


def thread(atomic_dict, iterations):
    data = make_data(iterations)

    def my_reduce_avg(key, current, new) -> tuple[int, int]:
        if type(new) is not tuple:
            new = (new, 1)
        if current is NOT_FOUND:
            return new
        current_total, current_count = current
        new_total, new_count = new
        return current_total + new_total, current_count + new_count

    atomic_dict.reduce(data, my_reduce_avg)


def threaded_avg(threads_num, thread_target):
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

    for k, (tot, cnt) in atomic_dict.fast_iter():
        atomic_dict[k] = tot / cnt

    total = 0
    count = 0
    for k, avg in atomic_dict.fast_iter():
        total += avg
        count += 1
    return total / count


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


print("\nAveraging using the built-in dict with a single thread:")
took_dict, average = time_and_run(builtin_dict_avg)
print(f" - Took {took_dict:.3f}s ({average=:.2f})")

thread_counts = [1, 2, 3, 4]
print("\nAveraging using cereggii.AtomicDict.reduce():")
for count in thread_counts:
    took, average = time_and_run(threaded_avg, count, thread)
    print(f" - Took {took:.3f}s with {count} threads ({took_dict / took:.1f}x faster, {average=:.2f})")
