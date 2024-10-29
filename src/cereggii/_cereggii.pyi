from collections.abc import Iterable, Iterator, Callable
from typing import NewType, SupportsComplex, SupportsFloat, SupportsInt

Key = NewType("Key", object)
Value = NewType("Value", object)
Cancel = NewType("Cancel", object)

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

class AtomicDict:
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
        chunk_size: int = 0,
    ) -> None:
        """
        Aggregate the values in this dictionary with those found in `iterable`,
        as computed by `aggregate`.

        The `aggregate` function takes as input a key, the value currently stored
        in the dictionary,
        and the new value from `iterator`. It returns the aggregated value.

        !!! note

            The `aggregate` function **must** be:

            - **total** &mdash; it should handle both the case in which the key is present and in which it is not
            - **state-less** &mdash; you should not rely on the number of times this function is called (it will be
            called at least once for each item in `iterable`, but there is no upper bound)

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

            First, an intermediate result is aggregated into a thread-local dictionary, and then applied to the shared
            `AtomicDict`. This can greatly reduce contention when the keys in the input are repeated.
        """

    def compact(self) -> None: ...
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

class AtomicRef:
    """An object reference that may be updated atomically."""

    def __init__(self, initial_value: object | None = None): ...
    def compare_and_set(self, expected: object, desired: object) -> bool:
        """
        Atomically read the current value of this `AtomicRef`:

          - if it is `expected`, then replace it with `desired` and return `True`
          - else, don't change it and return `False`.
        """

    def get(self) -> object:
        """
        Atomically read the current value of this `AtomicRef`.
        """

    def get_and_set(self, desired: object) -> object:
        """
        Atomically swap the value of this `AtomicRef` to `desired` and return
        the previously stored value.
        """

    def set(self, desired: object):  # noqa: A003
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

    def get_handle(self) -> AtomicInt64Handle:
        """
        Get a thread-local handle for this `AtomicInt64`.

        When using a thread-local handle, you can improve the performance of
        your application.
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

class AtomicInt64Handle(AtomicInt64):
    """
    A thread-local handle for an [`AtomicInt64`][cereggii._cereggii.AtomicInt64].
    It behaves exactly like `AtomicInt64`, but provides some performance benefits.

    You cannot instantiate an `AtomicInt64Handle` directly.
    Create it by calling [`AtomicInt64.get_handle`][cereggii._cereggii.AtomicInt64.get_handle]:
    ```python
    my_handle = my_atomic_int.get_handle()
    ```
    """
