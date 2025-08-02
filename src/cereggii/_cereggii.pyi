from collections.abc import Callable, Iterable, Iterator
from typing import Any, Self, SupportsComplex, SupportsFloat, SupportsInt

Number = SupportsInt | SupportsFloat | SupportsComplex

NOT_FOUND: object
""" A singleton object.
Used in `AtomicDict` to signal that a key was not found. """
ANY: object
""" A singleton object.
Used in `AtomicDict` as input for an unconditional update (upsert). """
EXPECTATION_FAILED: object
""" A singleton object.
Used in `AtomicDict` to return that an operation was aborted due to a
failed expectation. """
ExpectationFailed: Exception

class ThreadHandle[T](T):
    """
    A thread-local handle for an object.
    Acts as a proxy for the handled object.
    It behaves exactly like the object it handles, and provides some performance
    benefits.

    !!! tip
        Make sure the thread that created an instance of ThreadHandle
        is the only thread that uses the handle.

    !!! note
        ThreadHandle is immutable: once instantiated, you cannot change the
        handled object. If you need a mutable shared reference to an object,
        take a look at [AtomicRef][cereggii._cereggii.AtomicRef].

    !!! warning
        ThreadHandle does not enforce thread locality.
        You may be able to share one handle among several threads, but this is
        not the intended usage.

        Furthermore, the intended performance gains would be lost if the handle
        is used by multiple threads.

        Sharing a ThreadHandle among multiple threads may become unsupported
        in a future release.

    **How does ThreadHandle improve performance?**

    In free-threading Python, each object has new internal structures that
    have become necessary for the interpreter to stay correct in the face of
    multithreaded object use.
    While they are necessary for correctness, they may slow down an individual
    thread's access to an object if that object is used by multiple threads.
    ThreadHandle solves this performance problem by removing the contention
    on the new object's internal structures.

    The new structures mentioned are essentially a shared reference counter and
    a mutex, individually created with each object, in addition to a
    thread-local reference counter, which was already present in previous
    versions of Python.
    This is a very simplified explanation of these changes, to learn more about
    them, please refer to [PEP 703 – Making the Global Interpreter Lock Optional
    in CPython](https://peps.python.org/pep-0703/).

    Since ThreadHandle is also an object, it also has its own shared and local
    reference counters.
    When a thread makes a call to a method of ThreadHandle, the interpreter
    implicitly increments the local reference counter and ThreadHandle
    proxies the call to the handled object.
    Once the call is completed, the interpreter decrements the local reference
    counter of ThreadHandle.

    If ThreadHandle wasn't used and the handled object was used by multiple
    threads, the reference counting operations would have used the object's
    shared reference counter.
    Operations on this counter are atomic and more computationally expensive.

    If a ThreadHandle is used by multiple threads, then reference
    counting operations on the handle itself would use the shared reference
    counter and nullify the performance gains.
    """

class AtomicDict[Key, Value]:
    """
    A concurrent dictionary (hash table).

    Features of AtomicDict:

    1. you don't need an external lock to synchronize changes (mutations) to the dictionary:

        1. you don't have to manually guard your code against deadlocks (reentrancy-caused deadlocks can still be an
        issue)
        2. when `AtomicDict` is correctly configured (setting `min_size` so that no resizing occurs), even if the OS
        decides to interrupt or terminate a thread which was accessing an `AtomicDict`, all remaining threads will
        continue to make progress

    2. mutations are atomic and can be aborted or retried under contention
    3. scalability:

        1. TODO
        2. for some workloads scalability is already quite good: see
        [`AtomicDict.reduce`][cereggii._cereggii.AtomicDict.reduce].


    !!! note

        The special
        [`cereggii.NOT_FOUND`][cereggii.NOT_FOUND],
        [`cereggii.ANY`][cereggii.ANY], and
        [`cereggii.EXPECTATION_FAILED`][cereggii.EXPECTATION_FAILED]
        objects cannot be used as keys nor values.
    """

    def __init__(self, initial: dict = {}, *, min_size: int | None = None, buffer_size: int = 4):
        """
        Correctly configuring the `min_size` parameter avoids resizing the `AtomicDict`.
        Inserts that spill over this size will not fail, but may require resizing.
        Resizing prevents concurrent mutations until completed.

        :param initial: A `dict` to initialize this `AtomicDict` with.

        :param min_size: The size initially allocated.

        :param buffer_size: The amount of entries that a thread reserves for future
            insertions. A larger value can help reducing contention, but may lead to
            increased fragmentation. Min: 1, max: 64.
        """
    # def __contains__(self, item: Key) -> bool: ...
    def __delitem__(self, key: Key) -> None:
        """
        Atomically delete an item:
        ```python
        del my_atomic_dict[key]
        ```
        """

    def __getitem__(self, key: Key) -> Value:
        """
        Atomically read the value associated with `key`:
        ```python
        my_atomic_dict[key]
        ```

        Also see [`get`][cereggii._cereggii.AtomicDict.get].
        """
    # def __ior__(self, other) -> None: ...
    # def __iter__(self) -> Iterable[Key, Value]: ...
    def __len__(self) -> int:
        """
        Get the number of items in this `AtomicDict`:
        ```python
        len(my_atomic_dict)
        ```

        This method is sequentially consistent.
        When invoked, it temporarily locks the `AtomicDict` instance to compute
        the result.
        If you need to invoke this method frequently while the dictionary is
        being mutated, consider using
        [`AtomicDict.approx_len`][cereggii._cereggii.AtomicDict.approx_len] instead.
        """
    # def __eq__(self, other) -> bool: ...
    # def __or__(self, other) -> AtomicDict: ...
    # def __ror__(self, other) -> AtomicDict: ...
    def __repr__(self) -> str: ...
    # def __reversed__(self) -> AtomicDict: ...
    def __setitem__(self, key: Key, value: Value) -> None:
        """
        Unconditionally set the value associated with `key` to `value`:
        ```python
        my_atomic_dict[key] = value
        ```

        !!! warning

            Use [`compare_and_set`][cereggii._cereggii.AtomicDict.compare_and_set]
            instead.

            When an item is inserted or updated with this usual Python idiom, it is
            not possible to know that the value currently associated with `key` is the
            one being expected -- it may be mutated by another thread before this
            mutation is applied.
            Use this method only when no other thread may be writing to `key`.
        """
    # def __sizeof__(self) -> int: ...
    # def __str__(self) -> str: ...
    # def __subclasshook__(self): ...
    # def clear(self) -> None: ...
    # def copy(self) -> AtomicDict: ...
    # @classmethod
    # def fromkeys(cls, iterable: Iterable[Key], value=None) -> AtomicDict: ...
    def get(self, key: Key, default: Value | None = None) -> Value:
        """
        Just like Python's [`dict.get`](https://docs.python.org/3/library/stdtypes.html#dict.get):
        ```python
        my_atomic_dict.get(key, default=cereggii.NOT_FOUND)
        ```

        !!! tip

            The special [`cereggii.NOT_FOUND`][cereggii.NOT_FOUND] object
            can never be inserted into an `AtomicDict`, so when it is returned,
            you are ensured that the key was not in the dictionary.

            Conversely, `None` can both be a key and a value.

        :param key: The key to be looked up.
        :param default: The value to return when the key is not found.
        :return: The value associated with `key`, or `default`.
        """
    # def items(self) -> Iterable[Key, Value]: ...
    # def keys(self) -> Iterable[Key]: ...
    # def pop(self, key: Key, default: Value = None) -> Value: ...
    # def popitem(self) -> Value: ...
    # def setdefault(self) -> None: ...
    # def update(self, other: dict, **kwargs) -> None:  # batch
    #     """See ``help(dict.update)``.
    #
    #     The only difference with the built-in update method is that ``other`` cannot
    #     be used as a key when calling in the ``**kwargs`` style, as for
    #     :method:`__init__`, because of the limitations of the ``**kwargs`` behavior.
    #
    #     Note that this is also batched, like :method:`batch_lookup`, in the sense
    #     that ``other`` will be inserted into the dictionary in whole inside the C
    #     extension code, before returning to the Python interpreter.
    #
    #     :returns: None
    #     """
    # def update_by(self, fn: Callable[[Key, Value, Cancel], Value]) -> None:
    #     """Update all values by function result.
    #
    #     Example::
    #
    #         foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
    #         foo.update_fn(lambda key, value, cancel: value + 1)
    #         assert foo == {'a': 2, 'b': 3, 'c': 4}
    #
    #     Updates can be canceled by having :param:`fn` return the special ``cancel``
    #     value instead::
    #
    #         foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
    #
    #         def spam(key, value, cancel):
    #             if key == 'a':
    #                 return cancel
    #             return value + 1
    #
    #         foo.update_by(spam)
    #         assert foo == {'a': 1, 'b': 3, 'c': 4}
    #
    #     Note that having :param:`fn` return ``cancel`` instead of the unchanged
    #     ``value`` may be more efficient: it avoids a redundant atomic write to memory.
    #
    #     :returns: None
    #     """
    # def values(self) -> Iterable[Value]: ...
    def compare_and_set(self, key: Key, expected: Value, desired: Value) -> None:
        """
        Atomically read the value associated with `key`:

         - if it is `expected`, then replace it with `desired`
         - else, don't change it and raise `ExpectationFailed`.

        !!! example

            **Insert only**

            The expected value can be [`cereggii.NOT_FOUND`][cereggii.NOT_FOUND], in
            which case the call will succeed only when the item is inserted, and
            not updated:

            ```python
            my_atomic_dict.compare_and_set(
                key="spam",
                expected=cereggii.NOT_FOUND,
                desired=42,
            )
            ```

        !!! example

            **Counter**

            Correct way to increment the value associated with `key`, coping with concurrent mutations:

            ```python
            done = False
            while not done:
                expected = my_atomic_dict.get(key, default=0)
                try:
                    my_atomic_dict.compare_and_set(key, expected, desired=expected + 1)
                except cereggii.ExpectationFailed:
                    # d[key] was concurrently mutated: retry
                    pass
                else:
                    done = True
            ```

            The [`reduce`][cereggii._cereggii.AtomicDict.reduce] method removes a lot of boilerplate.

        !!! tip

            This family of methods is the recommended way to mutate an `AtomicDict`.
            Though, you should probably want to use a higher-level method than `compare_and_set`, like
            [`reduce`][cereggii._cereggii.AtomicDict.reduce].


        :raises ExpectationFailed: If the found value was not `expected`.
        """

    def len_bounds(self) -> tuple[int, int]:
        """
        Get a lower and an upper-bound for the number of items stored in this `AtomicDict`.

        Also see [`AtomicDict.approx_len`][cereggii._cereggii.AtomicDict.approx_len].
        """

    def approx_len(self) -> int:
        """
        Retrieve the approximate length of this `AtomicDict`.

        Calling this method does not prevent other threads from mutating the
        dictionary.

        !!! note

            Differently from [`AtomicDict.__len__`][cereggii._cereggii.AtomicDict.__len__],
            this method does not return a sequentially consistent result.

            This is not so bad!

            Suppose you call [`AtomicDict.__len__`][cereggii._cereggii.AtomicDict.__len__]
            instead. You would get a result that was correct in between the invocation of
            `len()`. But, at the very next line, the result might get invalidated by another
            thread inserting or deleting an item.

            If you need to know the size of an `AtomicDict` while other threads are
            mutating it, calling `approx_len` should be more performant and still
            return a fairly good approximation.
        """

    def fast_iter(self, partitions=1, this_partition=0) -> Iterator[tuple[Key, Value]]:
        """
        A fast, not sequentially consistent iterator.

        Calling this method does not prevent other threads from mutating this
        `AtomicDict`.

        !!! danger

            This method can return sequentially-inconsistent results.

            Only use `fast_iter` when you know that no other thread is mutating
            this `AtomicDict`.

            Depending on the execution, the following may happen:

            - it can return the same key multiple times (with the same or different values)
            - an update 1, that happened strictly before another update 2, is not seen,
              but update 2 is seen.

        !!! tip

            If no other thread is mutating this `AtomicDict` while a thread is calling this method, then this
            method is safe to use.

        !!! example

            **Summing an array with partitioning**

            ```python
            n = 3
            partials = [0 for _ in range(n)]

            def iterator(i):
                current = 0
                for k, v in d.fast_iter(partitions=n, this_partition=i):
                    current += v
                partials[i] = current

            threads = [
                threading.Thread(target=iterator, args=(i,))
                for i in range(n)
            ]

            for t in threads:
                t.start()
            for t in threads:
                t.join()

            print(sum(partials))
            ```

            Also see the [partitioned iterations
            example](https://github.com/dpdani/cereggii/blob/dev/examples/atomic_dict/partitioned_iter.py).

        :param partitions: The number of partitions to split this iterator with.
            It should be equal to the number of threads that participate in the
            iteration.
        :param this_partition: This thread's assigned partition.
            Valid values are from 0 to `partitions`-1.
        """

    def batch_getitem(self, batch: dict, chunk_size: int = 128) -> dict:
        """Batch many lookups together for efficient memory access.

        The values provided in `batch` will be substituted with the values found
        in the `AtomicDict` instance, or with `cereggii.NOT_FOUND`.
        Notice no exception is thrown: the `cereggii.NOT_FOUND` object instead
        is the returned value for a key that wasn't present.

        Notice that the `cereggii.NOT_FOUND` object can never be inserted
        into an `AtomicDict`.

        The values themselves, provided in `batch`, will always be substituted.

        !!! example

            With:
            ```python
            foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
            ```

            calling
            ```python
            foo.batch_getitem({
                'a': None,
                'b': None,
                'f': None,
            })
            ```

            returns:
            ```python
            {
               'a': 1,
               'b': 2,
               'f': <cereggii.NOT_FOUND>,
            }
            ```

        :param chunk_size: subdivide the keys to lookup from `batch` in smaller chunks of size `chunk_size to prevent
        memory over-prefetching.
        :returns: the input `batch` dictionary, with substituted values.
        """

    def reduce(
        self,
        iterable: Iterable[tuple[Key, Value]],
        aggregate: Callable[[Key, Value, Value], Value],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `aggregate`.

        The `aggregate` function takes as input a key, the value currently stored
        in the dictionary,
        and the new value from `iterator`. It returns the aggregated value.

        Several specialized methods are available to perform common operations:

        - [`reduce_sum`][cereggii._cereggii.AtomicDict.reduce_sum]
        - [`reduce_and`][cereggii._cereggii.AtomicDict.reduce_and]
        - [`reduce_or`][cereggii._cereggii.AtomicDict.reduce_or]
        - [`reduce_max`][cereggii._cereggii.AtomicDict.reduce_max]
        - [`reduce_min`][cereggii._cereggii.AtomicDict.reduce_min]
        - [`reduce_list`][cereggii._cereggii.AtomicDict.reduce_list]
        - [`reduce_count`][cereggii._cereggii.AtomicDict.reduce_count]

        !!! note

            The `aggregate` function **must** be:

            - **total** &mdash; it should handle both the case in which the key is present and in which it is not
            - **state-less** &mdash; you should not rely on the number of times this function is called (it will be
            called at least once for each item in `iterable`, but there is no upper bound)
            - **commutative** and **associative** &mdash; the result must not depend on the order of calls to `aggregate`

        !!! example
            **Counter**

            ```python
            d = AtomicDict()

            data = [
                ("red", 1),
                ("green", 42),
                ("blue", 3),
                ("red", 5),
            ]

            def count(key, current, new):
                if current is cereggii.NOT_FOUND:
                    return new
                return current + new

            d.reduce(data, count)
            ```

        !!! info

            This method exploits the skewness in the data.

            First, an intermediate result is aggregated into a thread-local dictionary and then applied to the shared
            `AtomicDict`. This can greatly reduce contention when the keys in the input are repeated.
        """

    def reduce_sum(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `sum()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = value
            else:
                atomic_dict[key] += value
        ```

        Behaves exactly as if [`reduce`][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def sum_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return new
            return current + new

        d.reduce(..., sum_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_and(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `all()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = not not value
            else:
                atomic_dict[key] = atomic_dict[key] and (not not value)
        ```

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def and_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return not not new
            return current and (not not new)

        d.reduce(..., and_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_or(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `any()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = not not value
            else:
                atomic_dict[key] = atomic_dict[key] or (not not value)
        ```

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def or_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return not not new
            return current or (not not new)

        d.reduce(..., or_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_max(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `max()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = value
            else:
                atomic_dict[key] = max(value, atomic_dict[key])
        ```

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def max_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return new
            return max(new, current)

        d.reduce(..., max_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_min(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `min()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = value
            else:
                atomic_dict[key] = min(value, atomic_dict[key])
        ```

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def min_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return new
            return min(new, current)

        d.reduce(..., min_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_list(
        self,
        iterable: Iterable[tuple[Key, Value]],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `list()`.

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        def to_list(obj):
            if type(obj) is list:
                return obj
            return [obj]

        for key, value in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = to_list(value)
            else:
                atomic_dict[key] = to_list(atomic_dict[key]) + to_list(value)
        ```

        !!! Warning
            The order of the elements in the returned list is undefined.
            This method will put all the elements from the input in the resulting
            list: their presence is guaranteed, but the order is not.

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        def list_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return to_list(new)
            return to_list(current) + to_list(new)

        d.reduce(..., list_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def reduce_count(
        self,
        iterable: Iterable[Key] | dict[Any, int],
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        by counting the number of occurrences of each key.
        (This is similar to the behavior of
        [`collections.Counter`](https://docs.python.org/3/library/collections.html#collections.Counter).)

        !!! Note
            Differently from [reduce][cereggii._cereggii.AtomicDict.reduce], this
            method does not interpret the input as an iterable of key-value pairs,
            but rather as an iterable of keys.

        !!! Info "From a dict"
            A `dict[Any, int]` can also be used, instead of an `Iterable[Key]`.
            This follows the behavior of `collections.Counter`.

            ```python
            my_atomic_dict = AtomicDict({"spam": 1})
            my_atomic_dict.reduce_count({"spam": 10, "eggs": 2, "ham": 3})
            assert my_atomic_dict["spam"] == 11
            assert my_atomic_dict["eggs"] == 2
            assert my_atomic_dict["ham"] == 3
            ```

        Multiple threads calling this method would effectively parallelize this single-threaded program:

        ```python
        for key in iterable:
            if key not in atomic_dict:
                atomic_dict[key] = 1
            else:
                atomic_dict[key] += 1
        ```

        Behaves exactly as if [reduce][cereggii._cereggii.AtomicDict.reduce] had been called like this:

        ```python
        import itertools

        def sum_fn(key, current, new):
            if current is cereggii.NOT_FOUND:
                return new
            return current + new

        d.reduce(zip(..., itertools.repeat(1)), sum_fn)
        ```

        !!! tip

            The implementation of this operation is internally optimized. It is recommended to use this method
            instead of calling `reduce` with a custom function.
        """

    def get_handle(self) -> ThreadHandle[Self]:
        """
        Get a thread-local handle for this `AtomicDict`.

        When using a thread-local handle, you can improve the performance of
        your application.

        See [`ThreadHandle`][cereggii._cereggii.ThreadHandle] for more
        information on thread-local object handles.
        """

    def _debug(self) -> dict:
        """
        Provide some debugging information.

        For internal usage only.
        This method is subject to change without a deprecation notice.
        """

    def _rehash(self, o: object) -> int:
        """
        Rehash object `o` with `AtomicDict`'s internal hashing function.

        For internal usage only.
        This method is subject to change without a deprecation notice.
        """

class AtomicRef[T]:
    """An object reference that may be updated atomically."""

    def __init__(self, initial_value: T = None): ...
    def compare_and_set(self, expected: T, desired: T) -> bool:
        """
        Atomically read the current value of this `AtomicRef`:

          - if it is `expected`, then replace it with `desired` and return `True`
          - else, don't change it and return `False`.
        """

    def get(self) -> T:
        """
        Atomically read the current value of this `AtomicRef`.
        """

    def get_and_set(self, desired: T) -> T:
        """
        Atomically swap the value of this `AtomicRef` to `desired` and return
        the previously stored value.
        """

    def set(self, desired: T):  # noqa: A003
        """
        Unconditionally set the value of this `AtomicRef` to `desired`.

        !!! warning

            Use [`compare_and_set`][cereggii._cereggii.AtomicRef.compare_and_set]
            instead.

            When using this method, it is not possible to know that the value currently
            stored is the one being expected -- it may be mutated by another thread before
            this mutation is applied. Use this method only when no other thread may be
            writing to this `AtomicRef`.
        """

    def get_handle(self) -> ThreadHandle[Self]:
        """
        Get a thread-local handle for this `AtomicRef`.

        When using a thread-local handle, you can improve the performance of
        your application.

        See [`ThreadHandle`][cereggii._cereggii.ThreadHandle] for more
        information on thread-local object handles.
        """

class AtomicInt64(int):
    """An `int` that may be updated atomically.

    !!! warning

        AtomicInt64 is bound to 64-bit signed integers: each of its methods may
        raise `OverflowError`.

    `AtomicInt64` borrows part of its API from Java's `AtomicInteger`, so that it
    should feel familiar to use, if you're coming to Python from Java.
    It also implements most numeric magic methods, so that it should feel
    comfortable to use for Pythonistas.

    !!! note

        The hash of an `AtomicInt64` is independent of its value.
        Two `AtomicInt64`s may have the same hash, but hold different values.
        They may also have different hashes, but hold the same values.

        If you need to get the hash of the currently stored `int` value, you
        should do this:
        ```py
        hash(my_atomic_int.get())
        ```

        An `AtomicInt64` and all of its associated `AtomicInt64Handle`s share the same hash value.

    !!! note

        The following operations are supported by `int`, but not `AtomicInt64`:

        - `__itruediv__` (e.g. `my_atomic_int /= 3.14` &mdash; an `AtomicInt64` cannot be used to store floats)
        - `as_integer_ratio`
        - `bit_length`
        - `conjugate`
        - `from_bytes`
        - `to_bytes`
        - `denominator`
        - `numerator`
        - `imag`
        - `real`

        You can of course call [`get`][cereggii._cereggii.AtomicInt64.get] on `AtomicInt64` and then call the desired
        method on the standard `int` object.
    """

    def __init__(self, initial_value: int = 0): ...
    def compare_and_set(self, expected: int, desired: int) -> bool:
        """
        Atomically read the current value of this `AtomicInt64`:

          - if it is `expected`, then replace it with `desired` and return `True`
          - else, don't change it and return `False`.
        """

    def get(self) -> int:
        """
        Atomically read the current value of this `AtomicInt64`.
        """

    def set(self, desired: int) -> None:  # noqa: A003
        """
        Unconditionally set the value of this `AtomicInt64` to `desired`.

        !!! warning

            Use [`compare_and_set`][cereggii._cereggii.AtomicInt64.compare_and_set]
            instead.

            When using this method, it is not possible to know that the value currently
            stored is the one being expected -- it may be mutated by another thread before
            this mutation is applied. Use this method only when no other thread may be
            writing to this `AtomicInt64`.
        """

    def get_and_set(self, desired: int) -> int:
        """
        Atomically swap the value of this `AtomicInt64` to `desired` and return
        the previously stored value.
        """

    def increment_and_get(self, /, amount: int = 1) -> int:
        """
        Atomically increment this `AtomicInt64` by `amount` and return the
        incremented value.
        """

    def get_and_increment(self, /, amount: int = 1) -> int:
        """
        Like [`increment_and_get`][cereggii._cereggii.AtomicInt64.increment_and_get], but returns the
        value that was stored before applying this operation.
        """

    def decrement_and_get(self, /, amount: int = 1) -> int:
        """
        Atomically decrement this `AtomicInt64` by `amount` and return the
        decremented value.
        """

    def get_and_decrement(self, /, amount: int = 1) -> int:
        """
        Like [`decrement_and_get`][cereggii._cereggii.AtomicInt64.decrement_and_get], but returns the
        value that was stored before applying this operation.
        """

    def update_and_get(self, /, callable: Callable[[int], int]) -> int:
        """
        Atomically update the value currently stored in this `AtomicInt64` by applying
        `callable` and return the updated value.

        `callable` should be a function that takes one `int` parameter and
        returns an `int`.

        !!! warning

            The `callable` function must be **stateless**: it will be called at least
            once but there is no upper bound to the number of times it will be
            called within one invocation of this method.
        """

    def get_and_update(self, /, callable: Callable[[int], int]) -> int:
        """
        Like [`update_and_get`][cereggii._cereggii.AtomicInt64.update_and_get], but returns the
        value that was stored before applying this operation.
        """

    def get_handle(self) -> ThreadHandle[Self]:
        """
        Get a thread-local handle for this `AtomicInt64`.

        When using a thread-local handle, you can improve the performance of
        your application.

        See [`ThreadHandle`][cereggii._cereggii.ThreadHandle] for more
        information on thread-local object handles.
        """

    def __itruediv__(self, other):
        raise NotImplementedError

    def as_integer_ratio(self):
        raise NotImplementedError

    def bit_length(self):
        raise NotImplementedError

    def conjugate(self):
        raise NotImplementedError

    @classmethod
    def from_bytes(cls, *args, **kwargs):
        raise NotImplementedError

    def to_bytes(self, *args, **kwargs):
        raise NotImplementedError

    @property
    def denominator(self):
        raise NotImplementedError

    @property
    def numerator(self):
        raise NotImplementedError

    @property
    def imag(self):
        raise NotImplementedError

    @property
    def real(self):
        raise NotImplementedError

class AtomicEvent:
    """An atomic event based on pthread's condition locks."""

    def __init__(self):
        pass

    def wait(self):
        """Wait for this `AtomicEvent` to be set."""

    def set(self):
        """Atomically set this `AtomicEvent`."""

    def is_set(self):
        """Atomically check if this `AtomicEvent` is set."""
