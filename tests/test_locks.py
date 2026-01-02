import threading

from cereggii.locks import ReadersWriterLock
from .utils import TestingThreadSet


def test_rwl_multiple_readers():
    lock = ReadersWriterLock()
    num_readers = 5
    barrier = threading.Barrier(num_readers)

    @TestingThreadSet.repeat(num_readers)
    def readers():
        barrier.wait()
        with lock.reader():
            barrier.wait()
            # all readers should be in at the same time
            assert lock._readers == num_readers
            barrier.wait()

    readers.start_and_join()
    assert lock._readers == 0


def test_rwl_writer_lock_exclusive():
    lock = ReadersWriterLock()
    num_writers = 10
    barrier = threading.Barrier(num_writers)

    @TestingThreadSet.repeat(num_writers)
    def writer():
        barrier.wait()
        with lock.writer():
            assert lock._writer == threading.get_ident()

    writer.start_and_join()
    assert lock._writer == 0


def test_rwl_writer_blocks_readers():
    lock = ReadersWriterLock()
    barrier = threading.Barrier(2)
    reader_entered = threading.Event()
    writer_released = threading.Event()

    @TestingThreadSet.repeat(1)
    def writer_thread():
        with lock.writer():
            barrier.wait()
            writer_released.set()

    @TestingThreadSet.repeat(1)
    def reader_thread():
        barrier.wait()
        with lock.reader():
            assert writer_released.is_set()
            reader_entered.set()

    (writer_thread | reader_thread).start_and_join()
    assert reader_entered.is_set()

def test_rwl_readers_block_writer():
    lock = ReadersWriterLock()
    barrier = threading.Barrier(2)
    writer_entered = threading.Event()
    readers_released = threading.Event()

    @TestingThreadSet.repeat(1)
    def reader_thread():
        with lock.reader():
            barrier.wait()
            readers_released.set()

    @TestingThreadSet.repeat(1)
    def writer_thread():
        barrier.wait()
        with lock.writer():
            assert readers_released.is_set()
            writer_entered.set()

    (reader_thread | writer_thread).start_and_join()
    assert writer_entered.is_set()

def test_rwl_reentrancy():
    lock = ReadersWriterLock()
    with lock.reader():
        with lock.reader():
            pass

    with lock.writer():
        with lock.writer():
            pass

def test_rwl_manual_acquire_release():
    lock = ReadersWriterLock()
    r = lock.reader()
    r.acquire()
    r.release()

    w = lock.writer()
    w.acquire()
    w.release()

def test_rwl_concurrent_readers_and_writers():
    lock = ReadersWriterLock()
    num_readers = 10
    num_writers = 5
    loops = 100
    counter = 0
    barrier = threading.Barrier(num_readers + num_writers)
    
    @TestingThreadSet.repeat(num_readers)
    def readers():
        barrier.wait()
        for _ in range(loops * 3):
            with lock.reader():
                assert 0 <= counter <= num_writers * loops

    @TestingThreadSet.repeat(num_writers)
    def writers():
        barrier.wait()
        for _ in range(loops):
            with lock.writer():
                nonlocal counter
                counter += 1

    (readers | writers).start_and_join()
    assert counter == num_writers * loops
