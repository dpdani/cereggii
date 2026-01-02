import threading

from pytest import raises

from cereggii import AtomicInt64, call_once
from .utils import TestingThreadSet


def test_call_once():
    calls = AtomicInt64(0)
    n = 10
    barrier = threading.Barrier(n)

    @call_once
    def func():
        calls.increment_and_get()
        return 99

    @TestingThreadSet.repeat(n)
    def threads():
        barrier.wait()
        nine_nine = func()
        assert nine_nine == 99

    threads.start_and_join()
    assert calls.get() == 1


def test_call_once_exception():
    calls = AtomicInt64(0)
    n = 10
    barrier = threading.Barrier(n)

    class MyException(Exception):
        pass

    @call_once
    def func():
        calls.increment_and_get()
        raise MyException()

    @TestingThreadSet.repeat(n)
    def threads():
        barrier.wait()
        with raises(MyException):
            func()

    threads.start_and_join()
    assert calls.get() == 1
