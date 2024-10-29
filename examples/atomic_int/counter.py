import threading
import timeit

import cereggii


class Spam:
    def __init__(self):
        self.counter = 0


class AtomicSpam:
    def __init__(self):
        self.counter = cereggii.AtomicInt64(0)


def builtin_int():
    print("A counter using the built-in int.")

    spam = Spam()
    print(f"{spam.counter=}")

    def incr():
        for _ in range(5_000_000):
            spam.counter += 1

    threads = [threading.Thread(target=incr) for _ in range(3)]

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    print(f"{spam.counter=}")


def atomic_int():
    print("A counter using cereggii.AtomicInt64.")

    spam = AtomicSpam()
    print(f"{spam.counter.get()=}")

    def incr():
        for _ in range(5_000_000):
            spam.counter += 1

    threads = [threading.Thread(target=incr) for _ in range(3)]

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    print(f"{spam.counter.get()=}")


def atomic_int_with_handle():
    print("A counter using cereggii.AtomicInt64 and cereggii.AtomicInt64Handle.")

    spam = AtomicSpam()
    print(f"{spam.counter.get()=}")

    def incr():
        h = spam.counter.get_handle()
        for _ in range(5_000_000):
            h += 1

    threads = [threading.Thread(target=incr) for _ in range(3)]

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    print(f"{spam.counter.get()=}")


t_int = timeit.timeit(builtin_int, "import gc; gc.enable()", number=1)
print(f"Took {t_int:.2f} seconds.\n")
t_atomic = timeit.timeit(atomic_int, "import gc; gc.enable()", number=1)
print(f"Took {t_atomic:.2f} seconds ({t_int / t_atomic:.2f}x faster).\n")
t_handle = timeit.timeit(atomic_int_with_handle, "import gc; gc.enable()", number=1)
print(f"Took {t_handle:.2f} seconds ({t_int / t_handle:.2f}x faster).")
