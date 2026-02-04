# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import threading
from threading import Condition, RLock


class ReadersWriterLock:
    """
    A readers-writer lock (RW lock) implementation.

    This lock allows multiple readers to hold the lock simultaneously, as long
    as there are no writers. When a writer requests the lock, it blocks until
    all current readers have released it. While a writer holds the lock, no
    other readers or writers can acquire it.

    This implementation is reentrant: a reader or writer thread that already
    holds the lock can acquire it again, but every acquisition must be matched
    by a release.

    It is recommended to use a `with` statement to manage the lock.

    !!! example "Example usage"

        ```python
        lock = ReadersWriterLock()

        with lock.reader():
            # do a read

        with lock.writer():
            # do a write
        ```

    !!! warning

        Readers may not become writers and vice versa. That is, the following
        code will block forever:

        ```python
        lock = ReadersWriterLock()

        with lock.reader():
            with lock.writer():
                pass
        ```
    """

    class ReaderLock:
        """
        A lock object that provides shared access for readers.
        """

        def __init__(self, rw_lock: 'ReadersWriterLock'):
            self._rw_lock = rw_lock

        def acquire(self):
            """
            Acquire the reader lock.

            Blocks if a writer currently holds the lock.
            """
            with self._rw_lock._lock:
                while self._rw_lock._writer:
                    self._rw_lock._condition.wait()
                self._rw_lock._readers += 1

        def release(self):
            """
            Release the reader lock.
            """
            with self._rw_lock._lock:
                self._rw_lock._readers -= 1
                assert self._rw_lock._readers >= 0
                if self._rw_lock._readers == 0:
                    self._rw_lock._condition.notify_all()

        def __enter__(self):
            self.acquire()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.release()

    class WriterLock:
        """
        A lock object that provides exclusive access for writers.
        """

        def __init__(self, rw_lock: 'ReadersWriterLock'):
            self._rw_lock = rw_lock

        def acquire(self):
            """
            Acquire the writer lock.

            Blocks if any readers or another writer currently hold the lock.
            This method is reentrant for the current writer thread.
            """
            me = threading.get_ident()
            with self._rw_lock._lock:
                if self._rw_lock._writer == me:
                    # reentrant acquisition
                    self._rw_lock._writer_acquisitions += 1
                    return
                while self._rw_lock._readers > 0 or self._rw_lock._writer:
                    self._rw_lock._condition.wait()
                assert self._rw_lock._writer == 0
                assert self._rw_lock._writer_acquisitions == 0
                self._rw_lock._writer = me
                self._rw_lock._writer_acquisitions = 1

        def release(self):
            """
            Release the writer lock.
            """
            with self._rw_lock._lock:
                assert self._rw_lock._writer == threading.get_ident()
                self._rw_lock._writer_acquisitions -= 1
                assert self._rw_lock._writer_acquisitions >= 0
                if self._rw_lock._writer_acquisitions == 0:
                    self._rw_lock._writer = 0
                    self._rw_lock._condition.notify_all()

        def __enter__(self):
            self.acquire()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.release()

    def __init__(self):
        self._readers = 0
        self._writer = 0
        self._writer_acquisitions = 0
        self._lock = RLock()
        self._condition = Condition(self._lock)

    def reader(self) -> ReaderLock:
        """
        Return a reader lock object.
        """
        return self.ReaderLock(self)

    def writer(self) -> WriterLock:
        """
        Return a writer lock object.
        """
        return self.WriterLock(self)
