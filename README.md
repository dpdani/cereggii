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

*Arm disclaimer:* `aarch64` processors are generally not supported, but this library was successfully used with Apple
Silicon.

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
i = 0
i += 1
```

That is, if `i` is shared with multiple threads, and they attempt to modify `i`, the value of `i` after any
number (> 1) of writes, is undefined.

The following piece of code is instead thread-safe:

```python
import cereggii


i = cereggii.AtomicInt(0)
i += 1
print(i.get())
```

Also, consider the [counter example](./examples/atomic_int/counter.py) where three counter implementations are compared:

1. using a built-in `int`,
2. using `AtomicInt`, and
3. using `AtomicInt` with `AtomicIntHandle`.

```text
A counter using the built-in int.
spam.counter=0
spam.counter=5019655
Took 39.17 seconds.

A counter using cereggii.AtomicInt.
spam.counter.get()=0
spam.counter.get()=15000000
Took 36.78 seconds (1.07x faster).

A counter using cereggii.AtomicInt and cereggii.AtomicIntHandle.
spam.counter.get()=0
spam.counter.get()=15000000
Took 2.64 seconds (14.86x faster).
```

Notice that when using `AtomicInt` the count is correctly computed, and that using `AtomicInt.get_handle`
to access the counter greatly improves performance.
When using `AtomicIntHandle`, you should see your CPUs being fully used, because no implicit lock
prevents the execution of any thread. [^implicitlock]

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

[^implicitlock]: Put simply, in a free-threaded build,
the [global interpreter lock](https://docs.python.org/3/glossary.html#term-global-interpreter-lock) is substituted with
many per-object locks.

[^1]: This behavior ensures the hashing property that identity implies hash equality.

### An explanation of these claims

First, `a += 1` in CPython actually translates to more than one bytecode instruction, namely:

```text
LOAD_CONST               2 (1)
INPLACE_ADD              0 (a)
STORE_FAST               0 (a)
```

This means that between the `INPLACE_ADD` and the `STORE_FAST` instructions, the value of `a` may have been changed by
another thread, so that one or multiple increments may be lost.

Second, the performance problem.
How come the speedup?

If we look again at [the example code](./examples/atomic_int/counter.py), there are a couple of implicit memory
locations which are being contended by threads:

1. the reference count of `spam.count`;
2. the reference counts of the `int` objects themselves; and
3. the lock protecting the implicit `spam.__dict__`.

These contentions are eliminated by `AtomicInt`, cf.:

1. `spam.count` is accessed indirectly through an `AtomicIntHandle` which avoids contention on its reference count (it
   is contended only during the `.get_handle()` call);
2. this is avoided by not creating `int` objects during the increment;
3. again, using the handle instead of the `AtomicInt` itself avoids spurious contention, because `spam.__dict__` is not
   modified.

Also see colesbury/nogil#121.

## AtomicDict

You can see that there is some more performance to be gained by simply using `AtomicDict`, looking at the execution
of [the count keys example](./examples/atomic_dict/count_keys.py).

```text
Counting keys using the built-in dict.
Took 35.46 seconds.

Counting keys using cereggii.AtomicDict.
Took 7.51 seconds (4.72x faster).
```

Notice that the performance gain was achieved by simply wrapping the dictionary initialization with `AtomicDict`
(compare `Spam.d` and `AtomicSpam.d` in [the example source](./examples/atomic_dict/count_keys.py)).

The usage of `AtomicInt` provides correctness, regardless of the hashmap implementation.
But using `AtomicDict` instead of `dict` improves performance, even without using handles: writes to distinct keys do
not generate contention.

### Pre-sized dictionary, with partitioned iterations

`AtomicDict` provides some more features compared to Python's `dict`, in
the [partitioned iteration example](./examples/atomic_dict/partitioned_iter.py) two of them are shown:

1. pre-sizing, which allows for the expensive dynamic resizing of a hash table to be avoided, and
2. partitioned iterations, which allows to split the number of elements among threads.

```text
Insertion into builtin dict took 36.81s
Builtin dict iter took 17.56s with 1 thread.
----------
Insertion took 17.17s
Partitioned iter took 8.80s with 1 threads.
Partitioned iter took 5.03s with 2 threads.
Partitioned iter took 3.92s with 3 threads.
```

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
