# Using `reduce()` for Averaging

This example demonstrates a more advanced use case of [`AtomicDict.reduce()`][cereggii._cereggii.AtomicDict.reduce]
to aggregate key-value pairs across multiple threads.
We'll compare the performance of the built-in Python `dict` with `AtomicDict` in
both single-threaded and multithreaded scenarios.

This example has a similar structure to the [Using `reduce()` for Multithreaded Aggregation example](./reduce.md).
If you haven't read that one already, please consider reading it before continuing.

Differently from the other example whose goal was to perform multithreaded summation,
here the goal is multithreaded averaging.
This is a more challenging problem because of a limitation of `reduce()`.

The interesting part of this example is how we can turn a function that would not
be supported by the semantics of `reduce()`, into one that does.

## Source Code

The source code is available [on GitHub](https://github.com/dpdani/cereggii/blob/main/examples/atomic_dict/reduce_avg.py).

### Generating Data

Like in the previous example, we generate some synthetic data that makes it easy 
to check the result.
The expected output should always be `2.00`, plus or minus floats imprecision.

```python
--8<-- "examples/atomic_dict/reduce_avg.py:7:22"
```

#### Implicit Contention

There are two peculiar things we should mention here:

1. we don't use `random.choice`, but we instantiate a `Random()` object of which
we call `choice()`.
2. we wrap the values list with a mysterious `ThreadHandle()` 😶‍🌫️

As in the previous example, the `make_data()` function is called by every thread
that we start.
It can be surprising to see that there if we didn't involve the `Random()` instance
and the `ThreadHandle()` we would have seen severely degraded performance later on.

The reason is that we would be accessing two global objects when calling `random.choice()` 
(the module function) and `choice(values)`.
In free-threading Python, when objects are shared between threads, internal data
structures involving those objects are contented.
If we want good multithreading performance, we must strive to avoid sharing objects.

Threads would access those objects in a tight loop.
We need to take care of that contention:

1. instead of using the module-global `Random()` instance, we create one for each
   thread; and
2. instead of accessing the `values` list directly, we wrap it in a `ThreadHandle()`

The `ThreadHandle()` provided by cereggii, is an object that mediates calls to a
thread-shared object.
This proxying of calls through the thread-local handle avoids implicit shared object
contention.
To learn more about this kind of contention, please see [`ThreadHandle()`'s documentation][cereggii._cereggii.ThreadHandle].

These scenarios can be commonly encountered when dealing with free-threading performance
hits.

!!! Tip ""
    If you edit the example code and remove either one of these mitigations, you'll 
    see performance dropping.

### Back with Averaging

Now let's get back to why averaging is such a painful multithreading problem.
The core of the issue is in the division operation.
We cannot apply a division without regard for the order in which threads will
apply it.

The other issue with averaging is that it requires a total view of the dataset —
we can't incrementally compute the average.

These two restrictions are against the requirement of [`reduce()`][cereggii._cereggii.AtomicDict.reduce]
that 

> The `aggregate` function must be:
> 
> - [...]
> - **commutative** and **associative** — the result must not depend on the order
> of calls to `aggregate`

We simply cannot use `reduce()` to compute the average of our dataset.
But we can do the next-best thing and turn our non-commutative and non-associative
function into one that is!

Instead of computing the average with our function, we'll compute the sum of the
values and keep a count of the items in the input.
The division itself will then be delayed until all threads are done.

Turning some problem into a summation problem is a good approach for making it 
multithreading-friendly.

### Friends Again

Without further ado, let's see the custom `aggregate` function for `reduce()`:

```python
--8<-- "examples/atomic_dict/reduce_avg.py:44:51"
```

This is not as simple as the function we've seen in the [previous example](./reduce.md).
Since we essentially want to keep two counters, it's fair enough that the output
of the function is `tuple[int, int]`.

The next line might be more surprising.
What is the type of `new`?
It's either:

- `int` — we're reading from the input data; or
- `tuple[int, int]` — we're reading a value stored in the dictionary by another thread.

This is a reminder that we need to help `reduce()` by covering all possible cases
of our multithreaded scenario.

The rest should be pretty straightforward if you followed the previous example on `reduce()`.

### Dividing

Now that we computed the sum in a multithreading-friendly way, it's a good time
to look at the function that starts the threads.
It's been slightly modified from the previous example:

```python
--8<-- "examples/atomic_dict/reduce_avg.py:56:74"
```

The ending of the function is where we do the division.
For simplicity, we average again the values pertaining to different keys, but we
could've also kept them separate in order to check them separately.

The thing I want to point out here is that there is one constraint for where in
this code we can or cannot compute the division.
Namely, after calling `join()` on every thread.

If we did that after the call to `start()` and before the call to `join()` our
division here would be racing against the threads running `reduce()`.

### Single-Threaded Baseline

The single-threaded baseline is shown below.
I want to encourage you to ponder on this: is the multithreaded counterpart based
on `reduce()` a lot harder to grasp?
(Keep in mind that averaging the items in the dictionary at the end is just
for the sake of simplicity in checking the result.)

You might contend that I've kept it intentionally complicated, but I disagree.

```python
--8<-- "examples/atomic_dict/reduce_avg.py:25:38"
```

## Results

The `reduce()` function and the constraint of executing the division strictly after
the threads are done, provided us with correctness in our computation.
Here too, splitting the dataset among several threads helped with performance,
but we must recall what we learned in the [Implicit Contention section](#implicit-contention)
to avoid throwing it all away.

The results shown here have been run with a beta version of [free-threading](https://py-free-threading.github.io/)
Python 3.14.

```text
$ python -VV
Python 3.14.0b2 experimental free-threading build (main, Jun 13 2025, 18:57:59) [Clang 17.0.0 (clang-1700.0.13.3)]
```

!!! Note
    Speedup is relative to the single-threaded baseline.
    A value below 1.0x means it performed worse than the baseline.

```text
expected_avg=2.00

Averaging using the built-in dict with a single thread:
 - Took 0.831s (average=2.00)

Averaging using cereggii.AtomicDict.reduce():
 - Took 0.965s with 1 threads (0.9x faster, average=2.00)
 - Took 0.497s with 2 threads (1.7x faster, average=2.00)
 - Took 0.346s with 3 threads (2.4x faster, average=2.00)
 - Took 0.251s with 4 threads (3.3x faster, average=2.00)
```