from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Type

from . import AtomicDict
from ..atomic_event import AtomicEvent
from ..constants import ExpectationFailed, NOT_FOUND


_reserved = object()
_raised = object()
_tombstone_value = object()


@dataclass(slots=True, frozen=True)
class _CacheEntry:
    ready: AtomicEvent
    value: object
    exception: Exception | None = None
    expires: float | None = None

    @property
    def is_reservation(self):
        return self.value is _reserved


_tombstone = _CacheEntry(
    ready=AtomicEvent(),
    value=_tombstone_value,
)


class AtomicCache[K, V]:
    def __init__(self, fill: Callable[[K], V], ttl: float | None = None):
        _tombstone.ready.set()
        self._fill = fill
        self._ttl = ttl
        self._cache = AtomicDict[K, _CacheEntry]()

    def _do_fill(self, key: K, current: _CacheEntry | NOT_FOUND) -> _CacheEntry:
        reservation = _CacheEntry(ready=AtomicEvent(), value=_reserved)
        try:
            self._cache.compare_and_set(key, expected=current, desired=reservation)
        except ExpectationFailed:
            entry = self._cache.get(key, default=NOT_FOUND)
            assert entry is not NOT_FOUND
            return entry
        # reservation made
        value = _raised
        exception = None
        expires = None
        try:
            value = self._fill(key)
        except Exception as e:
            exception = e
        if self._ttl is not None:
            now = time.monotonic()
            expires = now + self._ttl if self._ttl is not None else None
        entry = _CacheEntry(ready=AtomicEvent(), value=value, exception=exception, expires=expires)
        entry.ready.set()
        self._cache.compare_and_set(key, expected=reservation, desired=entry)
        # this CAS must not fail
        reservation.ready.set()
        return entry

    def __getitem__(self, key: K) -> V:
        while True:
            entry = self._cache.get(key, default=NOT_FOUND)
            if (
                entry is NOT_FOUND
                or entry is _tombstone
                or (entry.expires is not None and entry.expires < time.monotonic())
            ):
                entry = self._do_fill(key, current=entry)
            if entry.is_reservation:
                entry.ready.wait()
            else:
                break
        if entry.exception is not None:
            raise entry.exception
        assert not entry.is_reservation
        return entry.value

    def __setitem__(self, key: K, value: V):
        raise NotImplementedError(
            "AtomicCache does not allow direct assignment "
            "so as to avoid race conditions. Please, use "
            "__getitem__() or get() instead, or consider "
            "using AtomicDict if you need more flexibility."
        )

    def invalidate(self, key: K):
        entry = self._cache.get(key, default=NOT_FOUND)
        while True:
            if entry is NOT_FOUND:
                return
            if entry.is_reservation:
                # being filled right now, wait for it to finish
                entry.ready.wait()
                entry = self._cache.get(key, default=NOT_FOUND)
                continue
            try:
                self._cache.compare_and_set(key, expected=entry, desired=_tombstone)
            except ExpectationFailed:
                entry = self._cache.get(key, default=NOT_FOUND)
            else:
                # CAS succeeded
                return

    @classmethod
    def memoize(cls):
        """
        Decorator for caching the return values of a function.
        :param func:
        :return:
        """
        def decorator(func):
            return cls.MemoizedFunction(cls, func)
        return decorator

    class MemoizedFunction[RV]:
        def __init__(self, cache_class: Type[AtomicCache], func: Callable[..., RV]):

            def _func(params: tuple[tuple, tuple]):
                args, kwargs = params
                return func(*args, **dict(kwargs))

            self._cache = cache_class[tuple[tuple, tuple], RV](_func)

        def __call__(self, *args, **kwargs):
            return self._cache[self._make_key(args, kwargs)]

        def invalidate(self, *args, **kwargs):
            return self._cache.invalidate(self._make_key(args, kwargs))

        @staticmethod
        def _make_key(args: tuple, kwargs: dict) -> tuple:
            if kwargs:
                # I dream of frozen dicts
                kwargs_list = list(kwargs.items())
                kwargs_list.sort()
                return args, tuple(kwargs_list)
            else:
                return args, ()
