# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import threading
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
                assert self._rw_lock._readers >= 0
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
        return self.ReaderLock(self)

    def writer(self) -> WriterLock:
        return self.WriterLock(self)
