from .atomic_ref import AtomicRef
from .constants import ExpectationFailed


class AtomicBool(AtomicRef):
    """
    A thread-safe boolean value.

    !!! example

        ```python
        from cereggii import AtomicBool

        flag = AtomicBool(False)

        # atomically set the value
        flag.set(True)

        # get the current value
        value = flag.get()

        # compare and set if value matches expected
        success = flag.compare_and_set(True, False)

        # get the old value and set a new one atomically
        old_value = flag.get_and_set(True)
        ```
    """

    def __init__(self, initial_value: bool = False):
        """
        Initialize an `AtomicBool` with the given value.

        :param initial_value: The initial boolean value. Defaults to `False`.
        :raises TypeError: If initial_value is not a bool.
        """
        if type(initial_value) is not bool:
            raise TypeError(f"{initial_value=!r} must be a bool")
        super().__init__(initial_value)

    def compare_and_set(self, expected: bool, desired: bool) -> bool:
        """
        Atomically read the current value of this `AtomicBool`:

        - if it is `expected`, then replace it with `desired` and return `True`
        - else, don't change it and return `False`.

        :raises TypeError: If expected or desired is not a bool.
        """
        if type(expected) is not bool:
            raise TypeError(f"{expected=!r} must be a bool")
        if type(desired) is not bool:
            raise TypeError(f"{desired=!r} must be a bool")
        return super().compare_and_set(expected, desired)

    def get_and_set(self, desired: bool) -> bool:
        """
        Atomically swap the value of this `AtomicBool` to `desired` and return
        the previously stored value.

        :param desired: The new boolean value to set.
        :returns: The previous boolean value.
        :raises TypeError: If desired is not a bool.
        """
        if type(desired) is not bool:
            raise TypeError(f"{desired=!r} must be a bool")
        return super().get_and_set(desired)

    def set_or_raise(self):
        """
        Set the value of this `AtomicBool` to `True` if it is currently `False`,
        or else raise an exception.

        :raises ExpectationFailed: If the value is already `True`.
        """
        if not self.compare_and_set(False, True):
            raise ExpectationFailed("already set to True")

    def reset_or_raise(self):
        """
        Reset the value of this `AtomicBool` to `False` if it is currently `True`,
        or else raise an exception.

        :raises ExpectationFailed: If the value is already `False`.
        """
        if not self.compare_and_set(True, False):
            raise ExpectationFailed("already set to False")

    def set(self, desired: bool):
        """
        Unconditionally set the value of this `AtomicBool` to `desired`.

        !!! warning

            Use [`compare_and_set()`][cereggii.atomic_bool.AtomicBool.compare_and_set]
            instead.

            When using this method, it is not possible to know that the value
            currently stored is the one being expected -- it may be mutated
            by another thread before this mutation is applied. Use this method
            only when no other thread may be writing to this `AtomicBool`.

        :param desired: The new boolean value to set.
        :raises TypeError: If desired is not a bool.
        """
        if type(desired) is not bool:
            raise TypeError(f"{desired=!r} must be a bool")
        super().set(desired)
