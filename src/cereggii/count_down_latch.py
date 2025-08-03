from .atomic_event import AtomicEvent
from .atomic_int import AtomicInt64


class CountDownLatch:
    """
    This mimics the `java.util.concurrent.CountDownLatch` class.
    Paraphrasing from Java's documentation:

    Implements a synchronization aid that allows one or more threads to wait until
    a set of operations being performed in other threads completes.

    The count starts at a given number, and threads decrement it until the count
    reaches zero, at which point waiting threads are released and any subsequent
    invocations of [`wait()`][cereggii.CountDownLatch.wait] return immediately.

    This is a one-shot phenomenon — the count cannot be reset.

    A CountDownLatch initialized to N can be used to make one thread wait until
    N threads have completed some action, or some action has been completed N
    times.
    """

    def __init__(self, count: int):
        """
        Initializes a CountDownLatch with a specified count value.

        If the initial count is set to zero, all calls to
        [`wait()`][cereggii.CountDownLatch.wait] return immediately.

        :param count: The number of times
            [`decrement()`][cereggii.CountDownLatch.decrement] or
            [`decrement_and_get()`][cereggii.CountDownLatch.decrement_and_get]
            must be called before threads can pass through
            [`wait()`][cereggii.CountDownLatch.wait].
            Must be a non-negative integer.

        :raises AssertionError: If the count is less than 0.
        """
        assert count >= 0, "count must be >= 0"
        super().__init__()
        self._count = AtomicInt64(count)
        self._reached_zero = AtomicEvent()
        if count == 0:
            self._reached_zero.set()

    def decrement_and_get(self) -> int:
        """
        Like [`decrement()`][cereggii.CountDownLatch.decrement], but additionally
        returns the observed count after the decrement.
        """
        while True:
            current_count = self._count.get()
            assert current_count >= 0
            if current_count == 0:
                return 0
            if self._count.compare_and_set(current_count, current_count - 1):
                current_count -= 1
                break
        if current_count == 0:
            self._reached_zero.set()
        return current_count

    def decrement(self) -> None:
        """
        Decreases the count of the latch by one and wakes up any waiting threads
        if the count reaches zero.

        If the current count equals zero, then nothing happens.
        """
        self.decrement_and_get()
        return None

    def get(self) -> int:
        """
        Returns the current count.
        """
        return self._count.get()

    def wait(self) -> None:
        """
        Block the current thread until the count reaches zero, due to invocations
        of [`decrement()`][cereggii.CountDownLatch.decrement] or
        [`decrement_and_get()`][cereggii.CountDownLatch.decrement_and_get] in
        other threads.
        """
        return self._reached_zero.wait()

    def __repr__(self):
        return f"<{self.__class__.__name__}(count={self._count.get()}) at {id(self):#x}>"
