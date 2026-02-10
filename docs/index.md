# cereggii

Thread synchronization utilities for Python.

<div class="grid cards" markdown>

-   :material-lightning-bolt: **Lock-Free Data Structures**

    Thread-safe building blocks like [`AtomicCache`][cereggii.atomic_dict.atomic_cache.AtomicCache] and [`AtomicDict`][cereggii._cereggii.AtomicDict] that scale without locking or external synchronization.

-   :material-memory: **Atomic Operations**

    Fine-grained control with [`AtomicInt64`][cereggii._cereggii.AtomicInt64] and [`AtomicRef`][cereggii._cereggii.AtomicRef] for building custom concurrent algorithms with compare-and-swap operations.

-   :material-sync: **Thread Coordination**

    Synchronization primitives including [`CountDownLatch`][cereggii.CountDownLatch], [`call_once`][cereggii.call_once], and [`ReadersWriterLock`][cereggii.locks.ReadersWriterLock] for coordinating complex multi-threaded workflows.

-   :material-speedometer: **True Parallelism**

    Unlock even more parallelism from [free-threading Python](https://py-free-threading.github.io/), with data structures that scale better than builtins across CPU cores.

</div>

See the [API Reference](./api/index.md) for more details.

## Installation

```shell
pip install cereggii
```
```shell
uv add cereggii
```

Cereggii is compatible with most environments where Python is supported.
Check the [compatibility matrix](./compatibility.md) for more details.
