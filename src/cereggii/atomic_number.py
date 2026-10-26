import math
import operator
from typing import Any

from .atomic_ref import AtomicRef


class AtomicNumber(AtomicRef):
    """A numeric value that may be updated atomically.

    `AtomicNumber` can hold Python integers, floats, complex numbers, and other numeric objects
    such as `Decimal` and `Fraction`. Supported operations depend on the stored type.

    !!! example "Basic Usage"

        ```python
        from cereggii import AtomicNumber

        number = AtomicNumber(1)

        # Use it like a regular number
        number += 1
        another = number * 2

        # Use the AtomicNumber API
        number.increment_and_get()
        number.compare_and_set(2, 2.5)
        ```

    !!! note

        The hash is based on object identity, not the stored value. Two
        `AtomicNumber` objects that compare equal can have different hashes.
         Use `hash(number.get())` to hash the current value.

    !!! note

        Each read or update is atomic. However, an operation
        involving two distinct `AtomicNumber` objects (e.g. `AtomicNumber(a) + AtomicNumber(b)`)
        reads them separately and doesn't provide a simultaneous snapshot of their values.

        When the same object occurs on both sides (e.g. `AtomicNumber(a) + AtomicNumber(a)`),
        that operation reuses one read of its value.
    """

    def __init__(self, initial_value: Any = 0):
        super().__init__(initial_value)

    def __hash__(self):
        return object.__hash__(self)

    def _resolve_operands(self, other):
        current = self.get()
        if other is self:
            # A self-operand must use the same current value instead of calling another get(), in case
            # that it has been changed between the two get() calls.
            operand = current
        elif isinstance(other, AtomicNumber):
            operand = other.get()
        else:
            operand = other
        return current, operand

    def _binary(self, other, operation, reversed=False):
        current, operand = self._resolve_operands(other)
        if reversed:
            return operation(operand, current)
        else:
            return operation(current, operand)

    # The 4 func below follow a similar flow:
    # first read the current value, then perform the comparison or operation,
    # and finally use the parent class's `compare_and_set` to atomically write the new value.
    # If that fails, the current value has been changed concurrently, so the process needs to be repeated.

    def _inplace(self, other, operation):
        while True:
            current, operand = self._resolve_operands(other)
            desired = operation(current, operand)
            if super().compare_and_set(current, desired):
                return self

    def compare_and_set(self, expected, desired):
        """
        Atomically read the current value of this `AtomicNumber`:

          - if it is `expected`, then replace it with `desired` and return `True`
          - else, don't change it and return `False`.
        """
        while True:
            current = self.get()
            if current != expected:
                return False
            if super().compare_and_set(current, desired):
                return True

    def get_and_update(self, update):
        """
        Like [`update_and_get`][cereggii.atomic_number.AtomicNumber.update_and_get], but returns the
        value that was stored before applying this operation.
        """
        while True:
            current = self.get()
            desired = update(current)
            if super().compare_and_set(current, desired):
                return current

    def update_and_get(self, update):
        """
        Atomically update the value currently stored in this `AtomicNumber` by applying
        `update` and return the updated value.

        `update` should be a function that takes the current numeric value and
        returns a new numeric value.

        !!! warning

            The `update` function must be **stateless**: it will be called at least
            once but there is no upper bound to the number of times it will be
            called within one invocation of this method.

            If the stored value is mutable, `update` must not mutate it in place.
        """
        while True:
            current = self.get()
            desired = update(current)
            if super().compare_and_set(current, desired):
                return desired

    def _inc_dec_internal(self, amount, operation):
        while True:
            current = self.get()
            desired = operation(current, amount)
            if super().compare_and_set(current, desired):
                return current, desired

    def increment_and_get(self, amount=1):
        """
        Atomically increment this `AtomicNumber` by `amount` and return the
        incremented value.
        """
        _, desired = self._inc_dec_internal(amount, operator.add)
        return desired

    def get_and_increment(self, amount=1):
        """
        Like [`increment_and_get`][cereggii.atomic_number.AtomicNumber.increment_and_get], but returns the
        value that was stored before applying this operation.
        """
        current, _ = self._inc_dec_internal(amount, operator.add)
        return current

    def decrement_and_get(self, amount=1):
        """
        Atomically decrement this `AtomicNumber` by `amount` and return the
        decremented value.
        """
        _, desired = self._inc_dec_internal(amount, operator.sub)
        return desired

    def get_and_decrement(self, amount=1):
        """
        Like [`decrement_and_get`][cereggii.atomic_number.AtomicNumber.decrement_and_get], but returns the
        value that was stored before applying this operation.
        """
        current, _ = self._inc_dec_internal(amount, operator.sub)
        return current

    # Binary operators

    def __add__(self, other):
        return self._binary(other, operator.add)

    def __radd__(self, other):
        return self._binary(other, operator.add, reversed=True)

    def __iadd__(self, other):
        return self._inplace(other, operator.add)

    def __sub__(self, other):
        return self._binary(other, operator.sub)

    def __rsub__(self, other):
        return self._binary(other, operator.sub, reversed=True)

    def __isub__(self, other):
        return self._inplace(other, operator.sub)

    def __mul__(self, other):
        return self._binary(other, operator.mul)

    def __rmul__(self, other):
        return self._binary(other, operator.mul, reversed=True)

    def __imul__(self, other):
        return self._inplace(other, operator.mul)

    def __matmul__(self, other):
        return self._binary(other, operator.matmul)

    def __rmatmul__(self, other):
        return self._binary(other, operator.matmul, reversed=True)

    def __imatmul__(self, other):
        return self._inplace(other, operator.matmul)

    def __truediv__(self, other):
        return self._binary(other, operator.truediv)

    def __rtruediv__(self, other):
        return self._binary(other, operator.truediv, reversed=True)

    def __itruediv__(self, other):
        return self._inplace(other, operator.truediv)

    def __floordiv__(self, other):
        return self._binary(other, operator.floordiv)

    def __rfloordiv__(self, other):
        return self._binary(other, operator.floordiv, reversed=True)

    def __ifloordiv__(self, other):
        return self._inplace(other, operator.floordiv)

    def __mod__(self, other):
        return self._binary(other, operator.mod)

    def __rmod__(self, other):
        return self._binary(other, operator.mod, reversed=True)

    def __imod__(self, other):
        return self._inplace(other, operator.mod)

    def __lshift__(self, other):
        return self._binary(other, operator.lshift)

    def __rlshift__(self, other):
        return self._binary(other, operator.lshift, reversed=True)

    def __ilshift__(self, other):
        return self._inplace(other, operator.lshift)

    def __rshift__(self, other):
        return self._binary(other, operator.rshift)

    def __rrshift__(self, other):
        return self._binary(other, operator.rshift, reversed=True)

    def __irshift__(self, other):
        return self._inplace(other, operator.rshift)

    def __and__(self, other):
        return self._binary(other, operator.and_)

    def __rand__(self, other):
        return self._binary(other, operator.and_, reversed=True)

    def __iand__(self, other):
        return self._inplace(other, operator.and_)

    def __xor__(self, other):
        return self._binary(other, operator.xor)

    def __rxor__(self, other):
        return self._binary(other, operator.xor, reversed=True)

    def __ixor__(self, other):
        return self._inplace(other, operator.xor)

    def __or__(self, other):
        return self._binary(other, operator.or_)

    def __ror__(self, other):
        return self._binary(other, operator.or_, reversed=True)

    def __ior__(self, other):
        return self._inplace(other, operator.or_)

    def __divmod__(self, other):
        return self._binary(other, divmod)

    def __rdivmod__(self, other):
        return self._binary(other, divmod, reversed=True)

    def __ipow__(self, other):
        return self._inplace(other, operator.pow)

    def __eq__(self, other):
        return self._binary(other, operator.eq)

    def __ne__(self, other):
        return self._binary(other, operator.ne)

    def __lt__(self, other):
        return self._binary(other, operator.lt)

    def __le__(self, other):
        return self._binary(other, operator.le)

    def __gt__(self, other):
        return self._binary(other, operator.gt)

    def __ge__(self, other):
        return self._binary(other, operator.ge)

    def __round__(self, ndigits=None):
        if ndigits is None:
            return round(self.get())
        else:
            return round(self.get(), ndigits)

    # Unary operators

    def __neg__(self):
        return operator.neg(self.get())

    def __pos__(self):
        return operator.pos(self.get())

    def __abs__(self):
        return abs(self.get())

    def __invert__(self):
        return operator.invert(self.get())

    def __bool__(self):
        return bool(self.get())

    def __int__(self):
        return int(self.get())

    def __float__(self):
        return float(self.get())

    def __complex__(self):
        return complex(self.get())

    def __index__(self):
        return operator.index(self.get())

    def __trunc__(self):
        return math.trunc(self.get())

    def __floor__(self):
        return math.floor(self.get())

    def __ceil__(self):
        return math.ceil(self.get())

    # Ternary operators

    def _resolve_power_operands(self, other, modulo):
        current = self.get()
        if other is self:
            other = current
        elif isinstance(other, AtomicNumber):
            other = other.get()

        if modulo is self:
            modulo = current
        elif isinstance(modulo, AtomicNumber):
            modulo = modulo.get()

        return current, other, modulo

    def __pow__(self, other, modulo=None):
        current, other, modulo = self._resolve_power_operands(other, modulo)
        return pow(current, other, modulo)

    def __rpow__(self, other, modulo=None):
        current, other, modulo = self._resolve_power_operands(other, modulo)
        return pow(other, current, modulo)
