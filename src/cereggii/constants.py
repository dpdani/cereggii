import warnings


try:
    from cereggii._cereggii import NOT_FOUND, ANY, EXPECTATION_FAILED, ExpectationFailed, ConcurrentUsageDetected
except ImportError as exc:
    NOT_FOUND = None
    ANY = None
    EXPECTATION_FAILED = None
    ExpectationFailed = Exception
    ConcurrentUsageDetected = Exception

    warnings.warn(str(exc), stacklevel=1)  # "UserWarning: No module named 'cereggii'" is expected during sdist build
