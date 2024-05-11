import threading
import time

import numpy as np
from cereggii import AtomicDict


keys_count = 2**25
keys = np.random.default_rng(seed=0).choice(2**5 * keys_count, size=keys_count, replace=False)  # distinct
keys = keys.tolist()

# import random
# keys = set()
# while len(keys) < keys_count:
#     keys.add(random.randrange(2**5 * keys_count))
# keys = list(keys)


def large_grow(dict_factory, num_threads):
    for n in num_threads:
        d = dict_factory()

        def thread(i, keys):
            for _ in range(i, keys_count, n):  # noqa: B023
                d[keys[_]] = None  # noqa: B023

        threads = []
        for i in range(n):
            t = threading.Thread(target=thread, args=(i, keys.copy()))
            threads.append(t)

        started = time.time()
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        took = time.time() - started
        print(f" - Took {took:2.3f}s with {n} threads.")


print(f"\nInserting {keys_count} keys using dict:")
large_grow(dict, [1, 3])
print(f"\nInserting {keys_count} keys using AtomicDict:")
large_grow(AtomicDict, [1, 3])
