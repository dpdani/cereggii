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
    """
    Thread-safe cache with atomic operations and optional TTL support.

    AtomicCache provides a lock-free, thread-safe caching mechanism that automatically
    fills cache entries on-demand using a provided fill function. It supports optional
    time-to-live (TTL) expiration and ensures that each key is filled exactly once,
    even when multiple threads access the same key concurrently.

    When multiple threads access the same key concurrently, only one thread will execute
    the fill function, while others wait for the result to be ready. This ensures that
    expensive computations are only performed once per key, even under high concurrency.
    Threads waiting for a result will be blocked until the result is ready.

    !!! example

        ```python
        from cereggii import AtomicCache
        import time

        def expensive_computation(key):
            return key

        cache = AtomicCache(expensive_computation, ttl=60.0)

        value = cache["spam"]  # computes and caches the result
        value = cache["spam"]  # doesn't call expensive_computation("spam") again

        time.sleep(61.0)  # wait for TTL to expire
        value = cache["spam"]  # recomputes the result

        cache.invalidate("spam")  # explicitly remove the entry from the cache
        value = cache["spam"]  # recomputes the result
        ```

    !!! example

        ```python
        from cereggii import AtomicCache

        @AtomicCache.memoize()
        def fib(n):
            if n <= 1:
                return n
            return fib(n - 1) + fib(n - 2)
        ```

    :param fill: A callable that takes a key and returns the corresponding value.
              This function is called once per key to populate the cache.
    :param ttl: Optional time-to-live in seconds. If specified, cached entries will
             expire after this duration and be refilled on the next access. Expired
             keys are removed lazily. Do not rely on this TTL for memory management.
    """
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
        """
        Get the cached value for a key, filling it if necessary.

        If the key is not in the cache or has expired, the fill function is called
        to compute the value. If multiple threads request the same key concurrently,
        only one thread will execute the fill function while others wait for the result.

        :param key: The key to look up in the cache.
        :returns: The cached or newly computed value.
        :raises Exception: If the fill function raised an exception for this key,
            that exception is re-raised.
        """
        while True:
            entry = self._cache.get(key, default=NOT_FOUND)
            if self._not_contains(entry):
                entry = self._do_fill(key, current=entry)
            if entry.is_reservation:
                entry.ready.wait()
            else:
                break
        if entry.exception is not None:
            raise entry.exception
        assert not entry.is_reservation
        return entry.value

    def _not_contains(self, entry: _CacheEntry) -> bool:
        return (
            entry is NOT_FOUND
            or entry is _tombstone
            or (entry.expires is not None and entry.expires < time.monotonic())
        )

    def __contains__(self, key: K) -> bool:
        """
        Check if a key exists in the cache and has not expired.

        ```python
        cache = AtomicCache(lambda x: x * 2)
        _ = cache[5]  # Populate cache
        assert 5 in cache
        assert 10 not in cache
        ```

        :param key: The key to check.
        :returns: `True` if the key exists in the cache and has not expired, `False` otherwise.
        """
        entry = self._cache.get(key, default=NOT_FOUND)
        return not self._not_contains(entry)

    def __setitem__(self, key: K, value: V):
        """
        Direct assignment is not supported in AtomicCache.

        This method raises `NotImplementedError` to prevent race conditions.
        Values should be computed through the fill function provided at initialization.

        :raises NotImplementedError: Always raised to prevent direct assignment.
        """
        raise NotImplementedError(
            "AtomicCache does not allow direct assignment so as to avoid race "
            "conditions. Please, use __getitem__() instead, or consider using "
            "AtomicDict if you need more flexibility."
        )

    def invalidate(self, key: K):
        """
        Remove a key from the cache, forcing it to be refilled on next access.

        If the key is currently being filled by another thread, this method waits
        for the fill operation to complete before invalidating. Subsequent accesses
        to the key will trigger a fresh fill operation.

        ```python
        cache = AtomicCache(lambda x: x * 2)
        result1 = cache[5]  # Computes 10
        cache.invalidate(5)
        result2 = cache[5]  # Recomputes 10
        ```

        :param key: The key to invalidate.
        """
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
    def memoize(cls, *args, **kwargs):
        """
        Decorator for caching the return values of a function.

        Creates a memoized version of a function that caches results based on
        the function's arguments. The decorated function will only compute each
        unique set of arguments once, returning cached results for subsequent calls.

        Additional parameters will be passed to the underlying
        [`AtomicCache`][cereggii.atomic_dict.atomic_cache.AtomicCache] constructor.

        ```python
        @AtomicCache.memoize(ttl=60.0)
        def expensive_function(x, y):
            return x ** y

        result1 = expensive_function(2, 10)  # computed
        result2 = expensive_function(2, 10)  # cached
        ```
        """

        def decorator(func):
            return cls.MemoizedFunction(cls, func, *args, **kwargs)

        return decorator

    class MemoizedFunction[RV]:
        """
        A memoized function wrapper that caches results based on arguments.

        This class is created by the `memoize()` decorator and provides thread-safe
        caching of function results with support for both positional and keyword arguments.
        """
        def __init__(self, cache_class: Type[AtomicCache], func: Callable[..., RV], *args, **kwargs):

            def _func(params: tuple[tuple, tuple]):
                _args, _kwargs = params
                return func(*_args, **dict(_kwargs))

            self._cache = cache_class[tuple[tuple, tuple], RV](_func, *args, **kwargs)

        def __call__(self, *args, **kwargs):
            """
            Call the memoized function with the given arguments.

            :param args: Positional arguments to pass to the function.
            :param kwargs: Keyword arguments to pass to the function.
            :returns: The cached or newly computed result.
            """
            return self._cache[self._make_key(args, kwargs)]

        def invalidate(self, *args, **kwargs):
            """
            Invalidate the cached result for specific arguments.

            ```python
            @AtomicCache.memoize()
            def compute(x, y):
                return x + y

            result = compute(1, 2)  # Cached
            compute.invalidate(1, 2)  # Remove from cache
            ```
            """
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
