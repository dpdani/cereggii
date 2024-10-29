import threading
import time

import cereggii


keys = 2**23
py_d = {}

started = time.time()
for _ in range(keys):
    py_d[_] = _
print(f"Insertion into builtin dict took {(time.time() - started) * 1000:.0f}ms")

started = time.time()
total = 0
for k, v in py_d.items():
    total += v
print(f"Builtin dict iter took {(time.time() - started) * 1000:.0f}ms with 1 thread ({total=}).")
print("----------")
del py_d

d = cereggii.AtomicDict(min_size=keys * 2)

started = time.time()
for _ in range(keys):
    d[_] = _
print(f"Insertion took {(time.time() - started) * 1000:.0f}ms")

for n in range(1, 4):

    partials = [0 for _ in range(n)]

    def iterator(i):
        current = 0
        for k, v in d.fast_iter(partitions=n, this_partition=i):
            current += v
        partials[i] = current

    threads = [threading.Thread(target=iterator, args=(_,)) for _ in range(n)]

    started = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    print(
        f"Partitioned iter took {(time.time() - started) * 1000:.0f}ms with {n} threads ({partials=} {sum(partials)=})."
    )
