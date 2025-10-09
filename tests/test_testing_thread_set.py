"""
This tests an internal cereggii testing utility.
"""

import pytest

from .utils import TestingThreadSet


def test_raises():
    @TestingThreadSet.repeat(3)
    def thread():
        assert False

    thread.start()
    with pytest.raises(AssertionError):
        thread.join()
