import itertools
from collections.abc import Callable, Iterable
from threading import Thread
from typing import overload


class ThreadSet:
    """
    A container for sets of threads.
    It provides boilerplate-removing utilities for handling standard library threads.
    See Python's [`threading.Thread`](https://docs.python.org/3/library/threading.html#thread-objects)
    documentation for general information on Python threads.
    """

    thread_factory = Thread

    def __init__(self, *threads: Thread):
        """
        You can initialize a `ThreadSet` with any threads you already defined:

        ```python
        class MyCustomThreadSubclass(threading.Thread):
            ...

        threads = ThreadSet(
            MyCustomThreadSubclass(...),
            threading.Thread(...),
        )
        threads.start_and_join()
        ```

        You can create unions of `ThreadSet`s:
        ```python
        readers = ThreadSet(...)
        readers.start()

        writers = ThreadSet(...)
        writers.start()

        (readers | writers).join()
        ```

        You can create a `ThreadSet` in a function definition, using [`with_args()`][cereggii.ThreadSet.with_args],
        [`repeat()`][cereggii.ThreadSet.repeat], or [`target`][cereggii.ThreadSet.target]:
        ```python
        @ThreadSet.repeat(10)  # will create 10 threads
        def workers():
            ...

        workers.start_and_join()
        ```

        With [`ThreadSet.target`][cereggii.ThreadSet.target], function typing
        information is kept:
        ```python
        @ThreadSet.target
        def reader(thread_id: int, color: str):
            ...

        @ThreadSet.target
        def writer(thread_id: int, color: str):
            ...

        threads = ThreadSet(
            reader(0, "red"),  # your IDE will show the correct types here
            reader(1, "green"),
            writer(2, "blue")
        )
        threads.start_and_join()
        ```
        """
        self._threads: set[Thread] = set(threads)

    class Args:
        """A class to hold function arguments to feed to threads."""

        def __init__(self, *args, **kwargs):
            self.args = args
            self.kwargs = kwargs

    @classmethod
    def with_args(cls, *args: Args) -> Callable[[Callable], "ThreadSet"]:
        """
        ```python
        @ThreadSet.with_args(ThreadSet.Args(1, color="red"), ThreadSet.Args(2, "blue"))
        def spam(thread_id, color):
            ...

        spam.start_and_join()
        ```
        """

        def decorator(target: Callable) -> ThreadSet:
            threads = []
            for arg in args:
                assert isinstance(arg, cls.Args)
                threads.append(cls.thread_factory(target=target, args=arg.args, kwargs=arg.kwargs))
            return cls(*threads)

        return decorator

    @classmethod
    def repeat(cls, times: int) -> Callable[[Callable], "ThreadSet"]:
        """
        ```python
        @ThreadSet.repeat(5)
        def workers():
            ...

        workers.start_and_join()
        ```
        """
        return cls.with_args(*itertools.repeat(cls.Args(), times))

    @overload
    @classmethod
    def range(cls, stop: int) -> Callable[[Callable], "ThreadSet"]: ...
    @overload
    @classmethod
    def range(cls, start: int, stop: int, step: int = 1) -> Callable[[Callable], "ThreadSet"]: ...
    @classmethod
    def range(cls, start: int, stop: int | None = None, step: int = 1) -> Callable[[Callable], "ThreadSet"]:
        """
        ```python
        identifiers = set()

        @ThreadSet.range(5)
        def workers(thread_id):
            identifiers.add(thread_id)

        workers.start_and_join()
        for i in range(5):
            assert i in identifiers
        ```
        The meaning of the parameters follows Python's [`range()`](https://docs.python.org/3.13/library/stdtypes.html#range).
        """
        if stop is None:
            start, stop = 0, start
        return cls.with_args(*(cls.Args(i) for i in range(start, stop, step)))

    @classmethod
    def target[**P](cls, target: Callable[P, None]) -> Callable[P, Thread]:
        """
        ```python
        @ThreadSet.target
        def spam(thread_id: int, color: str):
            ...

        threads = ThreadSet(
            spam(1, color="red"),
            spam(2, "blue"),
        )
        threads.start_and_join()
        ```
        """

        def inner(*args: P.args, **kwargs: P.kwargs) -> Thread:
            return cls.thread_factory(target=target, args=args, kwargs=kwargs)

        return inner

    def start(self):
        """Start the threads in this `ThreadSet`.

        Also see [`Thread.start()`](https://docs.python.org/3/library/threading.html#threading.Thread.start).
        """
        for t in self._threads:
            t.start()

    def join(self, timeout: float | None = None):
        """Join the threads in this `ThreadSet`.

        Also see [`Thread.join()`](https://docs.python.org/3/library/threading.html#threading.Thread.join).

        :param timeout: The timeout for each individual join to complete.
        """
        for t in self._threads:
            t.join(timeout)

    def start_and_join(self, join_timeout: float | None = None):
        """Start the threads in this `ThreadSet`, then join them."""
        self.start()
        self.join(join_timeout)

    def is_alive(self) -> Iterable[bool]:
        """Call [`Thread.is_alive()`](https://docs.python.org/3/library/threading.html#threading.Thread.is_alive)
        for each thread in this `ThreadSet`."""
        return (t.is_alive() for t in self._threads)

    def any_is_alive(self) -> bool:
        """
        ```python
        any(self.is_alive())
        ```
        """
        return any(self.is_alive())

    def all_are_alive(self) -> bool:
        """
        ```python
        all(self.is_alive())
        ```
        """
        return all(self.is_alive())

    def all_are_not_alive(self) -> bool:
        """
        ```python
        not self.any_is_alive()
        ```
        """
        return not self.any_is_alive()

    def __or__(self, other):
        """Returns a new `ThreadSet` containing all the threads in the operands,
        which must be `ThreadSet` instances.

        ```python
        threads = ThreadSet(...) | ThreadSet(...)
        ```
        """
        if not isinstance(other, self.__class__):
            raise TypeError(f"cannot make union of {self.__class__} and {other!r}")
        return self.__class__(*(set(self._threads) | set(other._threads)))

    def __ior__(self, other):
        """Adds the threads in the right operand into this `ThreadSet`.
        The right operand must be a `ThreadSet`.

        ```python
        threads = ThreadSet(...)
        threads |= ThreadSet(...)
        ```
        """
        if not isinstance(other, self.__class__):
            raise TypeError(f"cannot make union of {self.__class__} and {other!r}")
        self._threads |= set(other._threads)
        return self

    def __len__(self) -> int:
        """Returns the number of threads contained in this `ThreadSet`.

        ```python
        len(ThreadSet(...))
        ```
        """
        return len(self._threads)

    def __repr__(self) -> str:
        threads = str(self._threads)
        max_threads_len = 80
        if len(threads) > max_threads_len:
            threads = f"{threads[:max_threads_len]}... {len(self)} threads"
        return f"<{self.__class__.__qualname__}({threads})>"
