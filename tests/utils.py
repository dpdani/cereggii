from collections.abc import Callable, Iterable, Mapping
from threading import Thread
from typing import Any

from cereggii import ThreadSet
from pytest_reraise.reraise import Reraise


class ReraiseThread(Thread):
    def __init__(
        self,
        group: None = None,
        target: Callable[..., object] | None = None,
        name: str | None = None,
        args: Iterable[Any] = (),
        kwargs: Mapping[str, Any] | None = None,
        *,
        daemon: bool | None = None,
    ):
        self.reraise = Reraise()

        @self.reraise.wrap
        def wrapped_target(*args, **kwargs):
            with self.reraise:
                return target(*args, **kwargs)

        super().__init__(group, wrapped_target, name, args, kwargs, daemon=daemon)

    def join(self, timeout=None):
        super().join(timeout)
        self.reraise()


class TestingThreadSet(ThreadSet):
    thread_factory = ReraiseThread
