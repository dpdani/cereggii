# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import gc
from fractions import Fraction

import cereggii
from cereggii import AtomicInt64
from pytest import raises

from .utils import TestingThreadSet


def test_init():
    i = AtomicInt64()
    assert i.get() == 0
    i = AtomicInt64(100)
    assert i.get() == 100
    with raises(OverflowError):
        AtomicInt64(2**63)


def test_set():
    i = AtomicInt64()
    i.set(200)
    assert i.get() == 200
    with raises(OverflowError):
        i.set(2**63)


class Result:
    def __init__(self):
        self.result: int | object | None = 0


def test_cas():
    r = AtomicInt64()
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result, updated):
        result.result = r.compare_and_set(0, updated)

    obj_0 = 300
    id_0 = id(obj_0)
    obj_1 = 400
    id_1 = id(obj_1)
    TestingThreadSet(
        thread(result_0, obj_0),
        thread(result_1, obj_1),
    ).start_and_join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_swap():
    i = AtomicInt64(400)
    result_0 = Result()
    result_1 = Result()

    @TestingThreadSet.target
    def thread(result, updated):
        result.result = i.get_and_set(updated)

    obj_0 = 500
    id_0 = id(obj_0)
    obj_1 = 600
    id_1 = id(obj_1)
    TestingThreadSet(
        thread(result_0, obj_0),
        thread(result_1, obj_1),
    ).start_and_join()
    results = [result_0.result, result_1.result]

    assert 400 in results
    results.remove(400)
    assert results[0] in [obj_0, obj_1]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_increment_and_get():
    assert AtomicInt64(0).increment_and_get(1) == 1
    assert AtomicInt64(0).increment_and_get(None) == 1
    assert AtomicInt64(0).increment_and_get() == 1


def test_get_and_increment():
    i = AtomicInt64(0)
    assert i.get_and_increment(1) == 0
    assert i.get() == 1
    i = AtomicInt64(0)
    assert i.get_and_increment(None) == 0
    assert i.get() == 1
    i = AtomicInt64(0)
    assert i.get_and_increment() == 0
    assert i.get() == 1


def test_decrement_and_get():
    assert AtomicInt64(0).decrement_and_get(1) == -1
    assert AtomicInt64(0).decrement_and_get(None) == -1
    assert AtomicInt64(0).decrement_and_get() == -1


def test_get_and_decrement():
    i = AtomicInt64(0)
    assert i.get_and_decrement(1) == 0
    assert i.get() == -1
    i = AtomicInt64(0)
    assert i.get_and_decrement(None) == 0
    assert i.get() == -1
    i = AtomicInt64(0)
    assert i.get_and_decrement() == 0
    assert i.get() == -1


def test_get_and_update():
    i = AtomicInt64(0)
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 0
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 1
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 2
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 0

    with raises(TypeError):
        i.get_and_update(lambda _, mmm: _ + mmm)


def test_update_and_get():
    i = AtomicInt64(0)
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 1
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 2
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 0
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 1

    with raises(TypeError):
        i.update_and_get(lambda _, mmm: _ + mmm)


def test_get_handle():
    i = AtomicInt64()
    h = i.get_handle()
    assert isinstance(h, cereggii.ThreadHandle)
    assert isinstance(h.get_handle(), cereggii.ThreadHandle)


def test_dealloc():
    AtomicInt64()
    previous = None
    while (this := gc.collect()) != previous:
        previous = this


def test_hash():
    i = AtomicInt64(0)
    assert type(hash(i)) == int  # noqa: E721
    assert hash(i) != hash(0)
    h = i.get_handle()
    hh = h.get_handle()
    assert hash(i) == hash(h) == hash(hh)


def test_eq():
    zero = AtomicInt64(0)
    assert zero == 0
    assert zero == 0.0
    assert zero == Fraction(0, 1)
    assert zero == (0 + 0j)


def test_add():
    identity = AtomicInt64(0)
    assert identity + 1 == 1
    assert identity + 1.1 == 1.1
    assert identity + Fraction(-3, 2) == Fraction(-3, 2)
    assert identity + (1 + 1j) == (1 + 1j)

    with raises(TypeError):
        identity + None
    with raises(TypeError):
        identity + "spam"
    with raises(TypeError):
        identity + list()
    with raises(TypeError):
        identity + tuple()
    with raises(TypeError):
        identity + dict()
    with raises(TypeError):
        identity + set()


def test_sub():
    identity = AtomicInt64(0)
    assert identity - 1 == -1
    assert identity - 1.1 == -1.1
    assert identity - Fraction(-3, 2) == -Fraction(-3, 2)
    assert identity - (1 + 1j) == -(1 + 1j)

    with raises(TypeError):
        identity - None
    with raises(TypeError):
        identity - "spam"
    with raises(TypeError):
        identity - list()
    with raises(TypeError):
        identity - tuple()
    with raises(TypeError):
        identity - dict()
    with raises(TypeError):
        identity - set()


def test_mul():
    identity = AtomicInt64(1)
    assert identity * 0 == 0
    assert identity * 1.1 == 1.1
    assert identity * Fraction(-3, 2) == Fraction(-3, 2)
    assert identity * (1 + 1j) == (1 + 1j)
    assert identity * "spam" == "spam"
    assert identity * list() == list()
    assert identity * tuple() == tuple()

    with raises(TypeError):
        identity * None
    with raises(TypeError):
        identity * dict()
    with raises(TypeError):
        identity * set()


def test_remainder():
    assert AtomicInt64(100) % 9 == 1


def test_divmod():
    assert divmod(AtomicInt64(100), 9) == (11, 1)


def test_power():
    assert AtomicInt64(2) ** 2 == 4


def test_negative():
    assert -AtomicInt64(1) == -1
    assert -AtomicInt64(-1) == 1
    assert -AtomicInt64(0) == 0


def test_positive():
    assert +AtomicInt64(1) == 1
    assert +AtomicInt64(-1) == -1
    assert +AtomicInt64(0) == 0


def test_absolute():
    assert abs(AtomicInt64(-1)) == 1
    assert abs(AtomicInt64(1)) == 1
    assert abs(AtomicInt64(0)) == 0


def test_bool():
    i = bool(AtomicInt64(100))
    assert i
    assert type(i) == bool  # noqa: E721

    i = bool(AtomicInt64(0))
    assert not i
    assert type(i) == bool  # noqa: E721


def test_invert():
    assert -AtomicInt64(1) == -1


def test_lshift():
    assert AtomicInt64(1) << 64 == 1 << 64


def test_rshift():
    assert AtomicInt64(1) >> 1 == 0


def test_and():
    assert AtomicInt64(128) & 128 == 128
    assert AtomicInt64(128) & 64 == 0


def test_xor():
    assert AtomicInt64(128) ^ 128 == 0
    assert AtomicInt64(128) ^ 64 == 128 + 64


def test_or():
    assert AtomicInt64(128) | 128 == 128
    assert AtomicInt64(128) | 64 == 128 + 64


def test_int():
    i = int(AtomicInt64(100))
    assert i == 100
    assert type(i) == int  # noqa: E721


def test_float():
    f = float(AtomicInt64(100))
    assert f == 100.0
    assert type(f) == float  # noqa: E721


def test_iadd():
    i = AtomicInt64(0)
    i += 2**62
    assert i.get() == 2**62

    with raises(OverflowError):
        i += 2**62


def test_isub():
    i = AtomicInt64(0)
    i -= 2**62
    assert i.get() == -(2**62)

    with raises(OverflowError):
        i -= 2**62 + 1


def test_imul():
    i = AtomicInt64(1)
    i *= 2**62
    assert i.get() == 2**62

    with raises(OverflowError):
        i *= 2


def test_iremainder():
    i = AtomicInt64(100)
    i %= 9
    assert i == 1


def test_ipower():
    i = AtomicInt64(100)
    i **= 2
    assert i == 10000


def test_ilshift():
    i = AtomicInt64(100)
    i <<= 9
    assert i == 51200


def test_irshift():
    i = AtomicInt64(100)
    i >>= 9
    assert i == 0


def test_iand():
    i = AtomicInt64(100)
    i &= 9
    assert i == 0


def test_ixor():
    i = AtomicInt64(100)
    i ^= 9
    assert i == 109


def test_ior():
    i = AtomicInt64(100)
    i |= 9
    assert i == 109


def test_inplace_floor_divide():
    i = AtomicInt64(500)
    i //= 10
    assert i == 50


def test_inplace_true_divide():
    i = AtomicInt64()
    with raises(NotImplementedError):
        i /= 1


def test_floor_divide():
    result = AtomicInt64(500) // 10
    assert result == 50
    assert type(result) == int  # noqa: E721


def test_true_divide():
    result = AtomicInt64(500) / 10
    assert result == 50
    assert type(result) == float  # noqa: E721


def test_index():
    i = AtomicInt64(0)
    assert i.__index__() == 0


def test_as_integer_ratio():
    with raises(NotImplementedError):
        AtomicInt64().as_integer_ratio()


def test_bit_length():
    with raises(NotImplementedError):
        AtomicInt64().bit_length()


def test_conjugate():
    with raises(NotImplementedError):
        AtomicInt64().conjugate()


def test_from_bytes():
    with raises(NotImplementedError):
        AtomicInt64().from_bytes()


def test_to_bytes():
    with raises(NotImplementedError):
        AtomicInt64().to_bytes()


def test_denominator():
    with raises(NotImplementedError):
        AtomicInt64().denominator  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt64().denominator.__set__(0)


def test_numerator():
    with raises(NotImplementedError):
        AtomicInt64().numerator  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt64().numerator.__set__(0)


def test_imag():
    with raises(NotImplementedError):
        AtomicInt64().imag  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt64().imag.__set__(0)


def test_real():
    with raises(NotImplementedError):
        AtomicInt64().real  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt64().real.__set__(0)

def test_reflected_bin_ops():
    assert 0 & AtomicInt64(1) == 0
    assert divmod(3, AtomicInt64(2)) == (1, 1)
    assert 2 // AtomicInt64(1) == 2
    assert 2 << AtomicInt64(1) == 4
    with raises(TypeError):
        1 @ AtomicInt64(1)
    assert 3 % AtomicInt64(2) == 1
    assert 3 * AtomicInt64(2) == 6
    assert 0 | AtomicInt64(1) == 1
    assert 2 ** AtomicInt64(2) == 4
    assert 4 >> AtomicInt64(1) == 2
    assert 2 - AtomicInt64(1) == 1
    assert 2 / AtomicInt64(1) == 2
    assert 3 ^ AtomicInt64(1) == 2

def test_issue_76():
    assert AtomicInt64() + AtomicInt64() == 0
