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
