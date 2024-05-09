import threading
import time

import cereggii


keys = 2**26
py_d = {}

started = time.time()
for _ in range(keys):
    py_d[_] = _
print(f"Insertion into builtin dict took {time.time() - started:.2f}s")

started = time.time()
current = 0
for k, v in py_d.items():
    current = k + v
print(f"Builtin dict iter took {time.time() - started:.2f}s with 1 thread.")
print("----------")
del py_d

d = cereggii.AtomicDict(min_size=keys * 2)

started = time.time()
for _ in range(keys):
    d[_] = _
print(f"Insertion took {time.time() - started:.2f}s")

for n in range(1, 4):

    def iterator(i):
        current = 0
        for k, v in d.fast_iter(partitions=n, this_partition=i):  # noqa: B023
            current = k + v  # noqa

    threads = [threading.Thread(target=iterator, args=(_,)) for _ in range(1, n + 1)]

    started = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    print(f"Partitioned iter took {time.time() - started:.2f}s with {n} threads.")
