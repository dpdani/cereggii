from collections.abc import Iterator
from typing import Callable, NewType, SupportsComplex, SupportsFloat, SupportsInt

Key = NewType("Key", object)
Value = NewType("Value", object | None)
Cancel = NewType("Cancel", object)

Number = SupportsInt | SupportsFloat | SupportsComplex

NOT_FOUND: object
ANY: object
EXPECTATION_FAILED: object
ExpectationFailed: Exception

class AtomicDict:
    """A thread-safe dictionary (hashmap), that's almost-lock-free™."""

    def __init__(
        self, iterable: dict | None = None, /, *, min_size: int | None = None, buffer_size: int | None = None, **kwargs
    ):
        """Constructor method

        :param iterable: an iterable to initialize this dictionary with. For now,
            only `dict` can be supplied to this parameter.

        :param min_size: the size initially allocated. Using this
            parameter possibly avoids re-allocations and migrations during inserts.
            Inserts that spill over this size, will not fail, but may require
            re-allocations. Usage of this parameter is especially useful when the
            maximal required size is known.

        :param kwargs: key-value pairs with which to initialize the dictionary.
            This follows the behavior of built-in dictionary, but slightly differs:
            keys that are already parameters to this ``__init__`` method will not be
            considered, because of Python's ``**kwargs`` behavior.
            E.g. ``AtomicDict(initial_size=123)`` will not create a dictionary
            mapping ``initial_size`` to 123, but an empty one.
        """
    # def __contains__(self, item: Key) -> bool: ...
    def __delitem__(self, item: Key) -> None: ...
    def __getitem__(self, item: Key) -> object: ...
    # def __ior__(self, other) -> None: ...
    # def __iter__(self) -> Iterable[Key, Value]: ...
    def __len__(self) -> int: ...
    # def __eq__(self, other) -> bool: ...
    # def __or__(self, other) -> AtomicDict: ...
    # def __ror__(self, other) -> AtomicDict: ...
    def __repr__(self) -> str: ...
    # def __reversed__(self) -> AtomicDict: ...
    def __setitem__(self, key: Key, value: Value) -> None: ...
    # def __sizeof__(self) -> int: ...
    # def __str__(self) -> str: ...
    # def __subclasshook__(self): ...
    # def clear(self) -> None: ...
    # def copy(self) -> AtomicDict: ...
    # @classmethod
    # def fromkeys(cls, iterable: Iterable[Key], value=None) -> AtomicDict: ...
    def get(self, key: Key, default: Value | None = None): ...
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
    def compare_and_set(self, key: Key, expected: Value, desired: Value) -> None: ...
    def len_bounds(self) -> tuple[int, int]: ...
    def approx_len(self) -> int: ...
    def fast_iter(self, partitions=1, this_partition=0) -> Iterator[tuple[Key, Value]]: ...
    def batch_getitem(self, batch: dict, chunk_size: int = 128) -> dict:
        """Batch many lookups together for efficient memory access.

        Whatever the values provided in :param:`batch`, they will be substituted with
        the found values, or ``cereggii.NOT_FOUND``.
        Notice no exception is thrown: the ``cereggii.NOT_FOUND`` object instead
        is the returned value for a non-found key.
        Also notice that the ``cereggii.NOT_FOUND`` object can never be inserted
        into an ``AtomicDict``.

        The values themselves, provided in :param:`batch`, will always be substituted.

        An example call::

            foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
            foo.batch_getitem({
                'a': None,
                'b': None,
                'f': None,
            })

        which will return::

            {
               'a': 1,
               'b': 2,
               'f': <cereggii.NOT_FOUND>,
            }

        :returns: the input :param:`batch` dictionary, with substituted values.
        """
    # def aggregate(self, iterator: Iterator[tuple[Key, Value]], aggregation: Callable[[Key, Value, Value], Value])
    # -> None:
    #     """
    #     Aggregate the values in this dictionary with those found in ``iterator``, as computed by ``aggregation``.
    #
    #     The ``aggregation`` parameter expects a function that takes as input a key, the value currently stored
    #     in the dictionary, and the new value from ``iterator``, and then returns the aggregated value.
    #
    #     For instance, to aggregate counts::
    #
    #         d = AtomicDict()
    #
    #         it = [
    #             ("red", 1),
    #             ("green", 3),
    #             ("blue", 40),
    #             ("red", 1),
    #         ]
    #
    #         d.aggregate(it, lambda key, current, new:
    #             new if current is cereggii.NOT_FOUND else current + new
    #         )
    #     """
    def compact(self) -> None: ...
    def debug(self) -> dict: ...
    def rehash(self, o: object) -> int: ...

class AtomicRef:
    """An object reference that may be updated atomically."""

    def __init__(self, reference: object | None = None): ...
    def compare_and_set(self, expected: object, updated: object) -> bool: ...
    def get(self) -> object: ...
    def get_and_set(self, updated: object) -> object: ...
    def set(self, updated: object): ...  # noqa: A003

class AtomicInt(int):
    """An int that may be updated atomically."""

    def __init__(self, initial_value: int | None = None): ...
    def compare_and_set(self, expected: int, updated: int) -> bool: ...
    def get(self) -> int: ...
    def get_and_set(self, updated: int) -> int: ...
    def set(self, updated: int) -> None: ...  # noqa: A003
    def increment_and_get(self, amount: int | None = None) -> int: ...
    def get_and_increment(self, amount: int | None = None) -> int: ...
    def decrement_and_get(self, amount: int | None = None) -> int: ...
    def get_and_decrement(self, amount: int | None = None) -> int: ...
    def update_and_get(self, other: Callable[[int], int]) -> int: ...
    def get_and_update(self, other: Callable[[int], int]) -> int: ...
    def get_handle(self) -> AtomicIntHandle: ...
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

class AtomicIntHandle(AtomicInt):
    pass
