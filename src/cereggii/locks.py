# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

from threading import Condition, RLock


class ReadersWriterLock:
    class ReaderLock:
        def __init__(self, rw_lock: ReadersWriterLock):
            self._rw_lock = rw_lock

        def acquire(self):
            with self._rw_lock._lock:
                while self._rw_lock._writer:
                    self._rw_lock._condition.wait()
                self._rw_lock._readers += 1

        def release(self):
            with self._rw_lock._lock:
                self._rw_lock._readers -= 1
                if self._rw_lock._readers == 0:
                    self._rw_lock._condition.notify_all()

        def __enter__(self):
            self.acquire()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.release()

    class WriterLock:
        def __init__(self, rw_lock: ReadersWriterLock):
            self._rw_lock = rw_lock

        def acquire(self):
            with self._rw_lock._lock:
                while self._rw_lock._readers > 0 or self._rw_lock._writer:
                    self._rw_lock._condition.wait()
                self._rw_lock._writer = True

        def release(self):
            with self._rw_lock._lock:
                self._rw_lock._writer = False
                self._rw_lock._condition.notify_all()

        def __enter__(self):
            self.acquire()
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.release()

    def __init__(self):
        self._readers = 0
        self._writer = False
        self._lock = RLock()
        self._condition = Condition(self._lock)

    def reader(self) -> ReaderLock:
        return self.ReaderLock(self)

    def writer(self) -> WriterLock:
        return self.WriterLock(self)
