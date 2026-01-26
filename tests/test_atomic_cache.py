# SPDX-FileCopyrightText: 2026-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import threading

import pytest
from cereggii import AtomicInt64, AtomicCache
from cereggii.atomic_dict import atomic_cache
from cereggii.atomic_dict.atomic_cache import _tombstone

from .utils import TestingThreadSet


def test_getitem_fills_once_and_caches():
    calls = AtomicInt64(0)

    def fill(_):
        return calls.increment_and_get()

    cache = AtomicCache(fill)

    assert cache["spam"] == 1
    assert cache["spam"] == 1
    assert calls == 1
    assert "spam" in cache


def test_getitem_raises_and_caches_exception():
    calls = AtomicInt64(0)

    def fill(key):
        calls.increment_and_get()
        raise ValueError(key)

    cache = AtomicCache(fill)

    with pytest.raises(ValueError, match="bad"):
        cache["bad"]
    with pytest.raises(ValueError, match="bad"):
        cache["bad"]
    assert calls == 1
    assert "bad" in cache


def test_getitem_refills_after_invalidate():
    calls = AtomicInt64(0)

    def fill(_):
        return calls.increment_and_get()

    cache = AtomicCache(fill)

    assert cache["key"] == 1
    cache.invalidate("key")
    assert "key" not in cache
    assert cache["key"] == 2


def test_invalidate_noop_for_missing_key():
    calls = AtomicInt64(0)

    def fill(_):
        return calls.increment_and_get()

    cache = AtomicCache(fill)
    cache.invalidate("missing")
    assert calls == 0


def test_invalidate_waits_for_reservation():
    calls = AtomicInt64(0)
    started = threading.Event()
    allow_fill = threading.Event()
    barrier = threading.Barrier(2)

    def fill(_):
        calls.increment_and_get()
        started.set()
        allow_fill.wait()
        return "value"

    cache = AtomicCache(fill)

    @TestingThreadSet.repeat(1)
    def filler():
        assert cache["k"] == "value"

    @TestingThreadSet.repeat(1)
    def invalidator():
        barrier.wait()
        cache.invalidate("k")

    (filler | invalidator).start()
    started.wait()
    barrier.wait()
    allow_fill.set()
    filler.join()
    invalidator.join()

    assert cache._cache.get("k") is _tombstone

    refilled = cache["k"]
    assert refilled == "value"
    assert calls == 2


def test_concurrent_getitem_single_fill():
    calls = AtomicInt64(0)
    started = threading.Event()
    allow_fill = threading.Event()
    n = 5
    barrier = threading.Barrier(n)

    def fill(_):
        calls.increment_and_get()
        started.set()
        allow_fill.wait()
        return "value"

    cache = AtomicCache(fill)

    @TestingThreadSet.repeat(n)
    def workers():
        barrier.wait()
        assert cache["k"] == "value"

    workers.start()
    assert started.wait(timeout=1)
    allow_fill.set()
    workers.join()
    assert calls == 1


def test_ttl_expiration_triggers_refill(monkeypatch):
    calls = AtomicInt64(0)
    now = [100.0]

    def fake_monotonic():
        return now[0]

    monkeypatch.setattr(atomic_cache.time, "monotonic", fake_monotonic)

    def fill(_):
        return calls.increment_and_get()

    cache = AtomicCache(fill, ttl=10.0)

    assert cache["k"] == 1
    assert "k" in cache
    now[0] = 105.0
    assert cache["k"] == 1
    assert "k" in cache
    now[0] = 111.0
    assert "k" not in cache
    assert cache["k"] == 2
    assert calls == 2


def test_setitem_not_supported():
    cache = AtomicCache(lambda key: key)
    with pytest.raises(NotImplementedError):
        cache["k"] = "v"


def test_memoize_caches_and_invalidates():
    calls = AtomicInt64(0)

    @AtomicCache.memoize()
    def add(a, b):
        calls.increment_and_get()
        return a + b

    assert add(1, 2) == 3
    assert add(1, 2) == 3
    assert calls == 1

    add.invalidate(1, 2)
    assert add(1, 2) == 3
    assert calls == 2

    assert add(2, 3) == 5
    assert calls == 3


def test_memoize_kwargs_order_independent():
    calls = AtomicInt64(0)

    @AtomicCache.memoize()
    def add(a, b):
        calls.increment_and_get()
        return a + b

    assert add(a=1, b=2) == 3
    assert add(b=2, a=1) == 3
    assert calls == 1


def test_memoize_passes_ttl_to_atomic_cache(monkeypatch):
    calls = AtomicInt64(0)
    now = [100.0]

    def fake_monotonic():
        return now[0]

    monkeypatch.setattr(atomic_cache.time, "monotonic", fake_monotonic)

    @AtomicCache.memoize(ttl=10.0)
    def fill(_):
        return calls.increment_and_get()

    assert fill("k") == 1
    now[0] = 105.0
    assert fill("k") == 1
    now[0] = 111.0
    assert fill("k") == 2
    assert calls == 2


def test_contains_missing_key():
    cache = AtomicCache(lambda key: key)
    assert "missing" not in cache


def test_contains_present_key():
    cache = AtomicCache(lambda key: key * 2)
    assert cache["spam"] == "spamspam"
    assert "spam" in cache


def test_contains_does_not_fill():
    calls = AtomicInt64(0)

    def fill(key):
        calls.increment_and_get()
        return key

    cache = AtomicCache(fill)

    assert "never_accessed" not in cache
    assert calls == 0
