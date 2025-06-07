from cereggii import AtomicDict, AtomicInt64, AtomicRef, ThreadHandle


def test_init():
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
