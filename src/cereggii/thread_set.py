import itertools
from collections.abc import Callable, Iterable
from threading import Thread


class ThreadSet:
    def __init__(self, *threads: Thread):
        self._threads: set[Thread] = set(threads)

    class Args:
        def __init__(self, *args, **kwargs):
            self.args = args
            self.kwargs = kwargs

    @classmethod
    def with_args(cls, *args: Args) -> Callable[[Callable], "ThreadSet"]:
        """
        @ThreadSet.with_args(ThreadSet.Args(1, color="red"), ThreadSet.Args(2, "blue"))
        def spam(thread_id, color):
            ...

        spam.start_and_join()
        """

        def decorator(target: Callable) -> ThreadSet:
            threads = []
            for arg in args:
                assert isinstance(arg, cls.Args)
                threads.append(Thread(target=target, args=arg.args, kwargs=arg.kwargs))
            return cls(*threads)

        return decorator

    @classmethod
    def repeat(cls, times: int) -> Callable[[Callable], "ThreadSet"]:
        """
        @ThreadSet.repeat(5)
        def workers():
            ...

        workers.start_and_join()
        """
        return cls.with_args(*itertools.repeat(cls.Args(), times))

    @classmethod
    def target[**P](cls, target: Callable[P, None]) -> Callable[P, Thread]:
        """
        @ThreadSet.target
        def spam(thread_id: int, color: str):
            ...

        threads = ThreadSet(
            spam(1, color="red"),
            spam(2, "blue"),
        )
        threads.start_and_join()
        """
        def inner(*args: P.args, **kwargs: P.kwargs) -> Thread:
            return Thread(target=target, args=args, kwargs=kwargs)

        return inner

    def start(self):
        for t in self._threads:
            t.start()

    def join(self, timeout: float | None = None):
        for t in self._threads:
            t.join(timeout)

    def start_and_join(self, join_timeout: float | None = None):
        self.start()
        self.join(join_timeout)

    def is_alive(self) -> Iterable[bool]:
        return (t.is_alive() for t in self._threads)

    def any_is_alive(self) -> bool:
        return any(self.is_alive())

    def all_are_alive(self) -> bool:
        return all(self.is_alive())

    def all_are_not_alive(self) -> bool:
        return not self.any_is_alive()

    def __or__(self, other):
        if not isinstance(other, self.__class__):
            raise TypeError(f"cannot make union of {self.__class__} and {other!r}")
        return self.__class__(*(set(self._threads) | set(other._threads)))

    def __ior__(self, other):
        if not isinstance(other, self.__class__):
            raise TypeError(f"cannot make union of {self.__class__} and {other!r}")
        self._threads |= set(other._threads)
        return self

    def __len__(self) -> int:
        return len(self._threads)

    def __repr__(self) -> str:
        threads = str(self._threads)
        max_threads_len = 80
        if len(threads) > max_threads_len:
            threads = f"{threads[:max_threads_len]}... {len(self)} threads"
        return f"<{self.__class__.__qualname__}({threads})>"
