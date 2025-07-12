# Using `reduce()` for Multithreaded Aggregation

This example demonstrates how to use [`AtomicDict.reduce()`][cereggii._cereggii.AtomicDict.reduce]
to aggregate key-value pairs across multiple threads.
We'll compare the performance of the built-in Python `dict` with `AtomicDict` in 
both single-threaded and multithreaded scenarios.

The example consists of:

- synthetic dataset generation
- baseline single-threaded dictionary summation
- multithreaded implementation using [`AtomicDict.reduce()`][cereggii._cereggii.AtomicDict.reduce]
  and [`reduce_sum()`][cereggii._cereggii.AtomicDict.reduce_sum]
- a performance comparison between the approaches

In this example we feed some synthetic data into an AtomicDict to compute the
per-key sum of the values in the input.

## Source Code

The source code is available [on GitHub](https://github.com/dpdani/cereggii/blob/main/examples/atomic_dict/counting_reduce.py).
We'll briefly go through its structure.

### Generating Data

First, we have some functions generating synthetic data.
Essentially, we repeat keys 1–10 `size` times.
We use 5 as the value for each item in the data so that we have a predictable
total which we can later use to check the outputs.

```python
--8<-- "examples/atomic_dict/counting_reduce.py:7:18"
```

### Single-threaded Baseline

Next, we introduce a single-threaded function that uses Python's built-in `dict`.
This will be useful to compare our multithreaded implementation against a baseline.
This function is quite straightforward: make sure the keys are present in the dictionary
with an initial value of 0, proceed with summing the values in the input, and then
return the sum of the values in the dictionary.

```python
--8<-- "examples/atomic_dict/counting_reduce.py:21:28"
```

### Multithreaded Implementation

Now we let's go through the multithreaded implementation of `builtin_dict_sum`.
This is what's going on in the function below:

1. create a new `AtomicDict()`
2. split the dataset into equally sized partitions
3. instantiate and start threads
4. wait for threads to finish
5. sum the values in the dictionary

```python
--8<-- "examples/atomic_dict/counting_reduce.py:47:63"
```

!!! Note
    The variable `size` is intentionally set to 10! = 3628800 so that partitioning
    works cleanly with any number of threads between 1 and 10.
    A more serious program that doesn't use synthetic data should look for a better
    way to partition it.

What has been intentionally left out of the snippet above is the target function
that the threads actually run.
This is because we'll compare two variants of that: one uses a handwritten input function
for `reduce()` (the `aggregate` argument), while the other uses a specialized reduce
function that sums the values in the input data, exactly as the handwritten variant does.
As we'll see later, this results not only in more convenience but also better performance.

### Understanding `reduce()` — Using a Handwritten Function

[`AtomicDict.reduce()`][cereggii._cereggii.AtomicDict.reduce] applies a user-defined
function to every key-value pair in a thread-safe way.
This function, such as `my_reduce_sum()` below, receives:

- `key`: the key (first item) of the pair
- `current`: the current value in the dictionary (or `NOT_FOUND` if absent)
- `new`: the new value to aggregate

Note that [`NOT_FOUND`][cereggii._cereggii.NOT_FOUND] is a special object that cannot be
used as either key nor value in AtomicDict.
When `current is NOT_FOUND`, it means that `key` is not present in the dictionary and so,
in this case, it makes sense to return the `new` value.

```python
--8<-- "examples/atomic_dict/counting_reduce.py:31:39"
```

This is somewhat more compact than the single-threaded version based on `dict`.
The `reduce()` method offloads from us the reads and writes to the shared dictionary
so that we don't inadvertently introduce data races.

Since the dictionary may be shared with multiple threads, the current thread executing
`reduce()` may try to update an item and fail because another thread has updated
the same item.
To cope with this contention, `reduce()` will call `my_reduce_sum()` again, and
`current` will hold the value that the other thread had put into the shared
dictionary.

!!! Tip
    Do not assume a fixed number of calls to your function.
    It will be called at least once per input item, but potentially more.

#### Limitations of Reduce

Because of this implicit retry mechanism, `reduce()` imposes some limitations to
the `aggregate` input functions.
Most importantly, those functions need to be commutative and associative: the
final result must not depend on the order of application.

More on how to cope with this limitation in the [Using `reduce()` for Averaging example](./reduce-average.md).

### Specialized Function

Summation is a simple and common way of using `reduce()`.
Since it is so, a specialized version is available in [
`AtomicDict.reduce_sum()`][cereggii._cereggii.AtomicDict.reduce_sum].
The code using the specialized reduction is very compact:

```python
--8<-- "examples/atomic_dict/counting_reduce.py:42:44"
```

Multithreaded summation is a solved problem.
We believe it should be easily accessible, too.

## Results

The results shown here have been run with a beta version of free-threading Python 3.14.

```text
$ python -VV
Python 3.14.0b2 experimental free-threading build (main, Jun 13 2025, 18:57:59) [Clang 17.0.0 (clang-1700.0.13.3)]
```

!!! Note
    Speedup is relative to the single-threaded baseline.
    A value below 1.0x means it performed worse than the baseline.

### Specialization

Using the specialized `reduce_sum()` function can yield higher performance even for
single-threaded code.
With multiple threads, the difference becomes increasingly more significant.

```text
expected_total=18144000

Counting keys using the built-in dict with a single thread:
 - Took 0.428s (total=18144000)

Counting keys using cereggii.AtomicDict.reduce_sum():
 - Took 0.324s with 1 threads (1.3x faster, total=18144000)
 - Took 0.167s with 2 threads (2.6x faster, total=18144000)
 - Took 0.113s with 3 threads (3.8x faster, total=18144000)
 - Took 0.086s with 4 threads (5.0x faster, total=18144000)
```

### Using the handwritten reduce function

Looking at the runtime of the non-specialized function, we can observe lower performance.
It is recommended to use one of the specialized reduce functions, whenever possible.

If you're implementing a custom function to use with `reduce()`, make sure to read the
[documentation about its requirements][cereggii._cereggii.AtomicDict.reduce].

```text
expected_total=18144000

Counting keys using the built-in dict with a single thread:
 - Took 0.428s (total=18144000)

Counting keys using cereggii.AtomicDict.reduce():
 - Took 0.453s with 1 threads (0.9x faster, total=18144000)
 - Took 0.228s with 2 threads (1.9x faster, total=18144000)
 - Took 0.153s with 3 threads (2.8x faster, total=18144000)
 - Took 0.114s with 4 threads (3.7x faster, total=18144000)
```

### Comparison Table

| method                    | threads | time (s) | speedup |
|---------------------------|---------|----------|---------|
| built-in `dict`           | 1       | 0.428    | 1.0x    |
| `AtomicDict.reduce()`     | 1       | 0.453    | 0.9x    |
|                           | 2       | 0.228    | 1.9x    |
|                           | 3       | 0.153    | 2.8x    |
|                           | 4       | 0.114    | 3.7x    |
| `AtomicDict.reduce_sum()` | 1       | 0.324    | 1.3x    |
|                           | 2       | 0.167    | 2.6x    |
|                           | 3       | 0.113    | 3.8x    |
|                           | 4       | 0.086    | 5.0x    |

## Summary

- complying with the requirements of `reduce()` lets us easily write 
  data-race-free code
- `reduce_sum()` is both more convenient and more performant
- multithreading brings significant speedups when using `AtomicDict`

Refer to the [`AtomicDict.reduce()` documentation][cereggii._cereggii.AtomicDict.reduce]
for full details on writing compatible reduce functions.
