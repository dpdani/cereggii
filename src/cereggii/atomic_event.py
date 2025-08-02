import warnings


try:
    from cereggii import _cereggii
except ImportError as exc:  # building sdist (without compiled modules)

    class AtomicEvent:
        def __init__(self):
            print("dummy")

    warnings.warn(str(exc), stacklevel=1)  # "UserWarning: No module named 'cereggii'" is expected during sdist build

else:
    AtomicEvent = _cereggii.AtomicEvent
