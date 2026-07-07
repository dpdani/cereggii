import pytest
from cereggii import AtomicDict, AtomicInt64, AtomicRef, ThreadHandle
from pytest import raises


def test_init():
    ThreadHandle[bool](True)
    for o in [
        0,
        0.0,
        "0",
        [0, 1],
        (0, 1),
        AtomicDict(),
        AtomicInt64(),
        AtomicRef(),
    ]:
        ThreadHandle(o)


def test_reflected_bin_ops():
    assert 0 & ThreadHandle(1) == 0
    assert divmod(3, ThreadHandle(2)) == (1, 1)
    assert 2 // ThreadHandle(1) == 2
    assert 2 << ThreadHandle(1) == 4
    with raises(TypeError):
        1 @ ThreadHandle(1)
    assert 3 % ThreadHandle(2) == 1
    assert 3 * ThreadHandle(2) == 6
    assert 0 | ThreadHandle(1) == 1
    assert 2 ** ThreadHandle(2) == 4
    assert 4 >> ThreadHandle(1) == 2
    assert 2 - ThreadHandle(1) == 1
    assert 2 / ThreadHandle(1) == 2
    assert 3 ^ ThreadHandle(1) == 2


def test_get_handle_survives_allocation_failure():
    # get_handle() built its args tuple for ThreadHandle_init without a NULL
    # check, so an allocation failure (Py_BuildValue -> NULL) reached
    # PyArg_ParseTuple(NULL): SIGABRT on a debug build, SIGSEGV on release.
    # It must raise MemoryError instead of crashing.
    _testcapi = pytest.importorskip("_testcapi")
    if not hasattr(_testcapi, "set_nomemory"):
        pytest.skip("_testcapi.set_nomemory unavailable")

    # Drain the 1-element tuple free-list so Py_BuildValue("(O)") actually
    # allocates (otherwise it reuses a cached tuple and never hits the failure).
    hold = [(i,) for i in range(5000)]

    for factory in (AtomicRef, AtomicDict, AtomicInt64):
        for start in range(40):
            obj = factory()
            _testcapi.set_nomemory(start, start + 2)
            try:
                obj.get_handle()
            except MemoryError:
                pass
            finally:
                _testcapi.remove_mem_hooks()

    assert len(hold) == 5000  # keep the free-list drain alive to here
