import multiprocessing
import threading
import time

import cereggii


keys = 2**26
d = cereggii.AtomicDict(min_size=keys * 2)

started = time.time()
for _ in range(keys):
    d[_] = _
print(f"Insertion took {time.time() - started:.2f}s")

for n in range(1, multiprocessing.cpu_count() + 1):

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
    print(f"Fast iter took {time.time() - started:.2f}s with {n} threads.")
