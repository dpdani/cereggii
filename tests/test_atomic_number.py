import math
import operator
from decimal import Decimal
from fractions import Fraction

import pytest
import cereggii
from cereggii import AtomicNumber

from .utils import TestingThreadSet


@pytest.mark.parametrize('value', [2**100, 1.5, 2 + 3j, Decimal('1.25'), Fraction(1, 3)])
def test_numeric_types(value):
    number = AtomicNumber(value)
    assert isinstance(number, cereggii.AtomicRef)
    assert number.get() is value
    alias = number
    number += value
    assert number is alias
    assert number == value * 2


def test_cas():
    number = AtomicNumber()
    assert number == 0
    old = Decimal('1.25')
    number.set(old)
    assert not number.compare_and_set(Decimal('9.99'), 9)
    assert number.get() is old
    assert number.compare_and_set(Decimal('1.25'), 2**100)
    assert number.get_and_set(0) == 2**100


def test_add():
    number = AtomicNumber(14.0)
    assert number + 3 == 17.0
    assert 3 + number == 17.0
    assert number + AtomicNumber(3) == 17.0
    assert isinstance(number + 3, float)
    assert number == 14.0

    alias = number
    number += 3
    assert number is alias
    assert number == 17.0
    assert isinstance(number.get(), float)


def test_divide():
    number = AtomicNumber(13.2)
    assert number // 4 == 3
    assert 4 // number == 0
    assert number // AtomicNumber(4) == 3
    number //= AtomicNumber(4)
    assert number == 3


def test_unary_and_conversions():
    number = AtomicNumber(-3)
    assert -number == 3
    assert +number == -3
    assert abs(number) == 3
    assert ~number == 2
    assert int(number) == -3
    assert float(number) == -3.0
    assert complex(number) == -3 + 0j
    assert operator.index(number) == -3
    assert bool(number)
    assert not AtomicNumber(0)
    assert number == -3
    real = AtomicNumber(1.75)
    assert round(real) == 2
    assert round(real, 1) == 1.8
    assert math.floor(real) == 1
    assert math.ceil(real) == 2
    assert math.trunc(real) == 1
    with pytest.raises(TypeError):
        operator.index(real)


def test_comparisons():
    number = AtomicNumber(3)
    assert number == 3
    assert number == AtomicNumber(3)
    assert number != 4
    assert 2 < number < 4
    assert 3 <= number <= 3
    assert 4 > number > 2
    assert 3 >= number >= 3


def test_hash():
    number = AtomicNumber(3)
    other = AtomicNumber(3)
    assert number == other
    assert hash(number) != hash(other)

    number_set = {number}
    assert other not in number_set
    number += 1
    assert number in number_set
    number_set.remove(number)
    assert number not in number_set
    number -= 1

    number_dict = {number: 'value1'}
    assert other not in number_dict
    number += 1
    assert number_dict[number] == 'value1'
    number_dict[number] = 'value2'
    assert number_dict.pop(number) == 'value2'


def test_failed_operation():
    old = 12345
    number = AtomicNumber(old)
    with pytest.raises(ZeroDivisionError):
        number /= 0
    assert number.get() is old


def test_exception_raised():
    class Broken:
        def __add__(self, other):
            raise TypeError('unsupported')

    with pytest.raises(TypeError, match='unsupported'):
        AtomicNumber(Broken()) + 1


def test_self_operand():
    number = AtomicNumber(3)
    number += number
    assert number == 6


def test_handle():
    number = AtomicNumber(1.5)
    handle = number.get_handle()
    handle += 0.5
    assert number == 2.0
    assert handle == 2.0


@pytest.mark.parametrize(
    'left,right,expected',
    [
        (2, 0.5, 2.5),
        (0.5, 2, 2.5),
        (2, Fraction(1, 2), Fraction(5, 2)),
        (2, Decimal('0.5'), Decimal('2.5')),
    ],
)
def test_mixed_numeric_values(left, right, expected):
    assert AtomicNumber(left) + right == expected
    assert left + AtomicNumber(right) == expected
    assert AtomicNumber(left) + AtomicNumber(right) == expected


def test_concurrent_increment():
    number = AtomicNumber(0)

    @TestingThreadSet.repeat(4)
    def workers():
        nonlocal number
        for _ in range(1000):
            number += 1

    workers.start_and_join()
    assert number == 4000


def test_concurrent_self_add():
    number = AtomicNumber(1)

    @TestingThreadSet.repeat(4)
    def workers():
        nonlocal number
        for _ in range(1000):
            number += number

    workers.start_and_join()
    assert number == 2**4000
