# cereggii

Thread synchronization utilities for free-threaded Python.

This library provides some atomic data types which, in a multithreaded context, are generally more performant compared
to CPython's builtin types.

## Cereus greggii

<img src="./.github/cereggii.jpg" align="right">

The *Peniocereus Greggii* (also known as *Cereus Greggii*) is a flower native to Arizona, New Mexico, Texas, and some
parts of northern Mexico.

This flower blooms just one summer night every year and in any given area, all these flowers bloom in synchrony.

[Wikipedia](https://en.wikipedia.org/wiki/Peniocereus_greggii)

_Image credits: Patrick Alexander, Peniocereus greggii var. greggii, south of Cooke's Range, Luna County, New Mexico, 10
May 2018, CC0. [source](https://www.flickr.com/photos/aspidoscelis/42926986382)_

## Installing

*This library is experimental*

Using [@colesbury's original nogil fork](https://github.com/colesbury/nogil?tab=readme-ov-file#installation) is required
to use this library.
You can get it with pyenv:

```shell
pyenv install nogil-3.9.10-1
```

Then, you may fetch this library [from PyPI](https://pypi.org/project/cereggii):

```shell
pip install cereggii
```

If you happened to use a non-free-threaded interpreter, this library may not be able to run, and if it does, you will
see poor performance.

## AtomicInt

In Python (be it free-threaded or not), the following piece of code is not thread-safe:

```python
a = 0
a += 1
```

That is, if `a` is shared with multiple threads, and they attempt to modify `a`, the value of `a` after any
number (> 1) of writes, is undefined.

The following piece of code is instead thread-safe:

```python
a = cereggii.AtomicInt(0)
a += 1
```

Also, consider the following piece of code:

```python
import threading


class Spam:
    def __init__(self):
        self.counter = 0


spam = Spam()
print(f"{spam.counter=}")


def incr():
    for i in range(1_000_000):
        spam.counter += 1


t1 = threading.Thread(target=incr)
t2 = threading.Thread(target=incr)
t1.start()
t2.start()
t1.join()
t2.join()

print(f"{spam.counter=}")
```

The output you'll see onscreen is not known.
If you subsititute `self.counter = 0` with `self.counter = AtomicInt(0)`, you'll be guaranteed to see
`2_000_000`, and the program should run slightly faster.

If you make an additional modification, your program will run much faster:

```python
def incr():
    h = spam.counter.get_handle()

    for i in range(1_000_000):
        h += 1
```

When using `AtomicIntHandle`, you should see your CPUs being fully used.

`AtomicInt` borrows part of its API from Java's `AtomicInteger`, so that it should feel familiar to use, if you're
coming to Python from Java.
It also implements most numeric magic methods, so that it should feel comfortable to use for Pythonistas.
It tries to mimic Python's `int` as close as possible, with some caveats:

- it is bound to 64-bit integers, so you may encounter `OverflowError`;
- hashing is based on the `AtomicInt`'s address in memory, so two distinct `AtomicInt`s will have distinct hashes, even
  when they hold the same value (bonus feature: an `AtomicIntHandle` has the same hash of its
  corresponding `AtomicInt`); [^1]
- the following operations are not supported:
    - `__itruediv__` (an `AtomicInt` cannot be used to store floats)
    - `as_integer_ratio`
    - `bit_length`
    - `conjugate`
    - `from_bytes`
    - `to_bytes`
    - `denominator`
    - `numerator`
    - `imag`
    - `real`

[^1]: This behavior ensures the hashing property that identity implies hash equality.

### An explanation of these claims

First, `a += 1` in CPython actually translates to more than one bytecode instruction, namely:

```text
              0 LOAD_CONST               2 (1)
              2 INPLACE_ADD              0 (a)
              4 STORE_FAST               0 (a)
```

This means that between the `INPLACE_ADD` and the `STORE_FAST` instructions, the value of `a` may have been changed by
another thread, so that one or multiple increments may be lost.

Second, the performance problem.
How come the speedup?

If we look again at the code, there are a couple of implicit memory locations which are being contended by threads:

1. the reference count of `spam.count`;
2. the reference counts of the `int` objects themselves; and
3. the lock protecting the implicit `spam.__dict__`.

These contentions are eliminated by `AtomicInt`, cf.:

1. `spam.count` is accessed indirectly through an `AtomicIntHandle` which avoids contention on its reference count (it
   is contended only during the `.get_handle()` call);
2. this is avoided by not creating `int` objects during the increment;
3. again, using the handle instead of the `AtomicInt` itself avoids spurious contention.

Also see colesbury/nogil#121.

## AtomicDict

Currently, the implementation of `AtomicDict` is quite limited:

- (dpdani/cereggii#3) it can hold at most $2^{25}$ keys (~33.5M);
- (dpdani/cereggii#4) it does not support deletions (`del d[k]`);
- (dpdani/cereggii#5) it does not support dynamic resizing; and
- several common functionalities are missing.

You can see that there is some more performance to be gained by simply using `AtomicDict`, comparing the following two
programs.

The usage of `AtomicInt` provides correctness, regardless of the hashmap implementation.
But using `AtomicDict` instead of `dict` improves performance, even without using handles: writes to distinct keys do
not generate contention.

With `dict`:

```python
import threading

from cereggii import AtomicDict, AtomicInt


class Spam:
    def __init__(self):
        self.d = {k: AtomicInt(0) for k in range(10)}


spam = Spam()
print(
    f"{spam.d[0].get()=} {spam.d[1].get()=} {spam.d[2].get()=} {spam.d[3].get()=} {spam.d[4].get()=} "
    f"{spam.d[5].get()=} {spam.d[6].get()=} {spam.d[7].get()=} {spam.d[8].get()=} {spam.d[9].get()=} "
)


def incr():
    keys = list(range(10))
    for _ in range(5_000_000):
        spam.d[keys[_ % len(keys)]] += 1


threads = [threading.Thread(target=incr) for _ in range(3)]
for t in threads:
    t.start()

for t in threads:
    t.join()

print(
    f"{spam.d[0].get()=} {spam.d[1].get()=} {spam.d[2].get()=} {spam.d[3].get()=} {spam.d[4].get()=} "
    f"{spam.d[5].get()=} {spam.d[6].get()=} {spam.d[7].get()=} {spam.d[8].get()=} {spam.d[9].get()=} "
)
```

To use `AtomicDict`, substitute in `self.d = AtomicDict({k: AtomicInt(0) for k in range(10)})`.

## AtomicRef

You can use an `AtomicRef` when you have a shared variable that points to an object, and you need to change the
referenced object concurrently.

This is not available in the Python standard library and was initially implemented as part of `AtomicDict`.

```python
import cereggii


o = object()
d = {}
i = 0

r = cereggii.AtomicRef()
assert r.get() is None
assert r.compare_and_set(None, o)
assert r.get_and_set(d) == o
r.set(i)  # always returns None
```

## Experimental

This library is experimental and should not be used in a production environment.

After all, as of now, it requires a non-official fork in order to run.

Porting to free-threaded Python 3.13 (3.13t) is planned.
