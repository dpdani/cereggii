import threading
import timeit

from cereggii import AtomicDict, AtomicInt


class Spam:
    def __init__(self):
        self.d = {k: AtomicInt(0) for k in range(10)}


class AtomicSpam:
    def __init__(self):
        self.d = AtomicDict({k: AtomicInt(0) for k in range(10)})


def test_dict(spam):
    def incr():
        keys = list(range(10))
        for _ in range(5_000_000):
            spam.d[keys[_ % len(keys)]] += 1

    threads = [threading.Thread(target=incr) for _ in range(3)]
    for t in threads:
        t.start()

    for t in threads:
        t.join()


print("Counting keys using the built-in dict.")
t_dict = timeit.timeit("test_dict(Spam())", "import gc; gc.enable()", number=1, globals=globals())
print(f"Took {t_dict:.2f} seconds.\n")
print("Counting keys using cereggii.AtomicDict.")
t_atomic = timeit.timeit("test_dict(AtomicSpam())", "import gc; gc.enable()", number=1, globals=globals())
print(f"Took {t_atomic:.2f} seconds ({t_dict / t_atomic:.2f}x faster).")
