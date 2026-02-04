import functools

from . import AtomicEvent, AtomicRef


_not_set = object()


def call_once(func):
    """
    Decorator that ensures a function is called only once during the lifetime of
    a process, across all threads.

    If multiple threads call the decorated function simultaneously, only one
    thread (the "leader") will execute the function. Other threads will wait
    for the leader to finish and then return the same result.

    If the function raises an exception, the same exception will be re-raised
    in all threads.

    .. rubric:: Example

    The ``expensive_initialization_function`` below will be called only once,
    even if it is called from multiple threads concurrently.

    .. code-block:: python

        from cereggii import call_once

        @call_once
        def expensive_initialization_function():
            pass

    Since you may want the function to be unit-tested and wrapping it in a
    decorator may make things awkward, you can wrap it after the definition:

    .. code-block:: python

        from cereggii import call_once

        def _expensive_initialization_function():
            pass

        expensive_initialization_function = call_once(_expensive_initialization_function)

    :param func: The function to be called once.
    :return: A wrapper function that ensures ``func`` is executed only once.
    """
    result = _not_set
    exception = _not_set
    leader = AtomicRef(False)
    done = AtomicEvent()

    @functools.wraps(func)
    def once_wrapper(*args, **kwargs):
        nonlocal result, exception
        if not done.is_set():
            if leader.compare_and_set(expected=False, desired=True):
                # this thread is the leader
                try:
                    result = func(*args, **kwargs)
                except Exception as e:
                    exception = e
                done.set()
            done.wait()
        if exception is not _not_set:
            raise exception
        assert result is not _not_set
        return result

    return once_wrapper
