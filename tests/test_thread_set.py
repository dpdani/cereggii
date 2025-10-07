import threading

from cereggii import ThreadSet


def dummy_thread():
    pass


def test_basics():
    threads = [
        threading.Thread(target=dummy_thread)
        for _ in range(3)
    ]
    ts = ThreadSet(*threads)
    for t in threads:
        assert "initial" in repr(t)
        # apparently, there's no other way to get a thread's state :shrug:
    ts.start_and_join()
    for t in threads:
        assert "stopped" in repr(t)


def test_union():
    len_part = 3
    ts_a = ThreadSet(*[
        threading.Thread(target=dummy_thread)
        for _ in range(len_part)
    ])
    ts_b = ThreadSet(*[
        threading.Thread(target=dummy_thread)
        for _ in range(len_part)
    ])
    ts_union = ts_a | ts_b
    assert len(ts_union) == len_part * 2
    ts_union |= ThreadSet(*[
        threading.Thread(target=dummy_thread)
        for _ in range(len_part)
    ])
    assert len(ts_union) == len_part * 3
    for t in ts_union._threads:
        assert "initial" in repr(t)
    ts_union.start_and_join()
    for t in ts_union._threads:
        assert "stopped" in repr(t)


def test_join_timeout():
    def infinite_wait_thread(stop: threading.Event):
        stop.wait(3.14)

    event = threading.Event()
    ts = ThreadSet(
        threading.Thread(target=infinite_wait_thread, args=(event,))
    )
    ts.start_and_join(join_timeout=0.01)
    assert all(ts.is_alive())
    event.set()
    ts.join()
    assert not any(ts.is_alive())


def test_with_args():
    results = set()

    @ThreadSet.with_args(ThreadSet.Args(1, color="red"), ThreadSet.Args(2, "blue"))
    def spam(thread_id, color):
        results.add((thread_id, color))

    spam.start_and_join()
    assert (1, "red") in results
    assert (2, "blue") in results


def test_target():
    results = set()

    @ThreadSet.target
    def spam(thread_id: int, color: str):
        results.add((thread_id, color))

    ts = ThreadSet(
        spam(1, color="red"),
        spam(2, "blue"),
    )
    ts.start_and_join()
    assert (1, "red") in results
    assert (2, "blue") in results


def test_repeat():
    results = []
    times = 10

    @ThreadSet.repeat(times)
    def spam():
        results.append("spam")

    assert isinstance(spam, ThreadSet)
    assert len(spam) == times
    spam.start_and_join()
    assert results == ["spam" for _ in range(times)]
