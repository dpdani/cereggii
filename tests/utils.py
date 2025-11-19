import functools
import os
import subprocess
import sys
from collections.abc import Callable, Iterable, Mapping
from threading import Thread
from typing import Any, Literal

import pytest
from cereggii import ThreadSet
from pytest_reraise.reraise import Reraise


class ReraiseThread(Thread):
    def __init__(
        self,
        group: None = None,
        target: Callable[..., object] | None = None,
        name: str | None = None,
        args: Iterable[Any] = (),
        kwargs: Mapping[str, Any] | None = None,
        *,
        daemon: bool | None = None,
    ):
        self.reraise = Reraise()

        @self.reraise.wrap
        def wrapped_target(*args, **kwargs):
            with self.reraise:
                return target(*args, **kwargs)

        super().__init__(group, wrapped_target, name, args, kwargs, daemon=daemon)

    def join(self, timeout=None):
        super().join(timeout)
        self.reraise()


class TestingThreadSet(ThreadSet):
    __test__ = False

    thread_factory = ReraiseThread



def repeated_test(repetitions: int):
    def wrapper(test_func):
        @functools.wraps(test_func)
        def wrapped():
            for _ in range(repetitions):
                test_func()

        return wrapped

    return wrapper


def eventually_raises(expected_exception, repetitions: int):
    def wrapper(test_func):
        @functools.wraps(test_func)
        def wrapped():
            with pytest.raises(expected_exception):
                for _ in range(repetitions):
                    test_func()

        return wrapped

    return wrapper


def find_repetitions_count_helper(should: Literal["fail", "pass"], statistically_significant_rounds=1000, round_timeout=10, max_search_limit=1_000_000):
    # do a binary search to find the smallest repetitions count that makes the
    # test behave consistently, over a statistically significant number of rounds
    if should not in ('pass', 'fail'):
        raise ValueError("should not in ('pass', 'fail')")
    test_repeats_env_key = "CEREGGII_FRCH_REPEATS"

    def wrapper(test_func):

        @functools.wraps(test_func)
        def wrapped():
            # Get the test function's module and name for pytest execution
            test_module = test_func.__code__.co_filename
            test_name = test_func.__qualname__

            def run_test_with_repetitions(repetitions):
                """Run the test in a subprocess with a given repetitions count"""
                env = os.environ.copy()
                env[test_repeats_env_key] = str(repetitions)
                try:
                    result = subprocess.run(
                        [
                            sys.executable, "-m", "pytest",
                            test_module, "-k", test_name, "-q"
                        ],
                        env=env,
                        capture_output=True,
                        text=True,
                        timeout=round_timeout,
                        check=False,
                    )
                    return result.returncode == 0
                except subprocess.TimeoutExpired:
                    # timeouts count as failures
                    return False

            def test_is_consistent(repetitions):
                for _ in range(statistically_significant_rounds):
                    result = run_test_with_repetitions(repetitions)
                    if should == "pass":
                        # if any test fails, we can stop early
                        if not result:
                            return False
                    else:  # should == "fail"
                        # if any test passes, we can stop early
                        if result:
                            return False
                return True

            low = 1
            high = 1
            while True:
                print(f"Testing {high} repetitions...")
                if test_is_consistent(high):
                    break
                else:
                    low = high
                    high *= 2
                if high > max_search_limit:
                    print("Warning: Reached maximum search limit (1M repetitions)")
                    break
            best_repetitions = high
            while low < high:
                if high - low == 1:
                    best_repetitions = high
                    break
                mid = (low + high) // 2
                print(f"Testing {mid} repetitions...")
                if test_is_consistent(mid):
                    best_repetitions = mid
                    high = mid - 1
                else:
                    low = mid + 1
            print(f"Optimal repetitions count: {best_repetitions}")

        if repeats := os.getenv("CEREGGII_FRCH_REPEATS", default=None):
            repeats = int(repeats)
            return repeated_test(repeats)(test_func)
        return wrapped

    return wrapper
