# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0


from collections.abc import Iterable
from typing import Callable, NewType, Optional

try:
    from cereggii import _atomic_dict
except ImportError:  # building sdist (without compiled modules)
    pass

Key = NewType("Key", object)
Value = NewType("Value", object)
Cancel = NewType("Cancel", object)


class AtomicDict:
    """A thread-safe dictionary (hashmap), that's almost-lock-free™."""

    def __init__(self, iterable: Optional[Iterable] = None, *, initial_size: Optional[int] = None, **kwargs):  # noqa
        """Constructor method

        :param initial_size: the size initially allocated. Using this
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
        self._dict = object()
        if iterable is not None:
            d = dict(iterable)
            self.update(d)
        if kwargs:
            self.update(kwargs)

    def __contains__(self, item):
        dummy_batch = {item: None}
        self.batch_lookup(dummy_batch)
        return dummy_batch[item] != KeyError

    def __delitem__(self, item):
        pass

    # def __getitem__(self, item):
    #     dummy_batch = {item: None}
    #     self.batch_lookup(dummy_batch)
    #     if dummy_batch[item] == KeyError:
    #         raise KeyError(item)
    #     return dummy_batch[item]

    def __getitem__(self, item):
        return _atomic_dict.lookup(self._dict, item)

    # def __hash__(self):
    #     # leave it to the default hash implementation (identity)
    #     pass

    def __ior__(self, other) -> None:
        # return None, modify self with elements from self | other
        pass

    def __iter__(self):
        pass

    def __len__(self):
        # should this be precise?
        pass

    def __eq__(self, other):
        pass

    def __or__(self, other) -> "AtomicDict":
        # return a new AtomicDict, with elements from self | other
        pass

    def __ror__(self, other) -> "AtomicDict":
        return self | other

    def __repr__(self):
        return f"<{self.__class__.__qualname__}({self._dict!r})>"

    def __reversed__(self):
        pass

    def __setitem__(self, key, value):
        # will there be need for single-item updates?
        self.update({key: value})

    def __sizeof__(self):
        pass

    def __str__(self):
        return repr(self._dict)

    def __subclasshook__(self):
        pass

    def batch_lookup(self, batch: dict) -> dict:
        """Batch many lookups together for efficient memory access.

        Whatever the values provided in :param:`batch`, they will be substituted with
        the found values, or ``KeyError``. Notice no exception is thrown: the
        ``KeyError`` object instead is the returned value for a non-found key. If you
        have ``KeyError`` values in your :class:`AtomicDict`, you may have trouble
        distinguishing between a ``KeyError`` that implies a lookup failure,
        and a ``KeyError`` that was indeed found.

        The values themselves, provided in :param:`batch`, will always be substituted.

        An example call::

            foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
            foo.batch_lookup({
                'a': None,
                'b': None,
                'f': None,
            })

        which will return::

            {
               'a': 1,
               'b': 2,
               'f': KeyError,
            }

        :returns: the input :param:`batch` dictionary, with substituted values.
        """
        pass

    def clear(self):
        pass

    def copy(self):
        pass

    @classmethod
    def fromkeys(cls, iterable: Iterable, value=None):
        new = cls()
        for item in iterable:
            new[item] = value
        return new

    def get(self, key):
        pass

    def items(self):
        pass

    def keys(self):
        pass

    def pop(self, key, default=None):
        pass

    def popitem(self):  # in LIFO order
        pass

    def setdefault(self):
        pass

    def update(self, other: dict, **kwargs) -> None:  # batch
        """See ``help(dict.update)``.

        The only difference with the built-in update method is that ``other`` cannot
        be used as a key when calling in the ``**kwargs`` style, as for
        :method:`__init__`, because of the limitations of the ``**kwargs`` behavior.

        Note that this is also batched, like :method:`batch_lookup`, in the sense
        that ``other`` will be inserted into the dictionary in whole inside the C
        extension code, before returning to the Python interpreter.

        :returns: None
        """
        pass

    def update_by(self, fn: Callable[[Key, Value, Cancel], Value]) -> None:
        """Update all values by function result.

        Example::

            foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})
            foo.update_fn(lambda key, value, cancel: value + 1)
            assert foo == {'a': 2, 'b': 3, 'c': 4}

        Updates can be canceled by having :param:`fn` return the special ``cancel``
        value instead::

            foo = AtomicDict({'a': 1, 'b': 2, 'c': 3})

            def spam(key, value, cancel):
                if key == 'a':
                    return cancel
                return value + 1

            foo.update_by(spam)
            assert foo == {'a': 1, 'b': 3, 'c': 4}

        Note that having :param:`fn` return ``cancel`` instead of the unchanged
        ``value`` may be more efficient: it avoids a redundant atomic write to memory.

        :returns: None
        """
        pass

    def values(self):
        pass
