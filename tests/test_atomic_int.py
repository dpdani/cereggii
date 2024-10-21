# SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
#
# SPDX-License-Identifier: Apache-2.0

import gc
import threading
from fractions import Fraction

import cereggii
from cereggii import AtomicInt
from pytest import raises


def test_init():
    i = AtomicInt()
    assert i.get() == 0
    i = AtomicInt(100)
    assert i.get() == 100
    with raises(OverflowError):
        AtomicInt(2**63)


def test_set():
    i = AtomicInt()
    i.set(200)
    assert i.get() == 200
    with raises(OverflowError):
        i.set(2**63)


class Result:
    def __init__(self):
        self.result: int | object | None = 0


def test_cas():
    r = AtomicInt()
    result_0 = Result()
    result_1 = Result()

    def thread(result, updated):
        result.result = r.compare_and_set(0, updated)

    obj_0 = 300
    id_0 = id(obj_0)
    t0 = threading.Thread(target=thread, args=(result_0, obj_0))
    obj_1 = 400
    id_1 = id(obj_1)
    t1 = threading.Thread(target=thread, args=(result_1, obj_1))
    t0.start()
    t1.start()
    t0.join()
    t1.join()

    assert sorted([result_0.result, result_1.result]) == [False, True]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_swap():
    i = AtomicInt(400)
    result_0 = Result()
    result_1 = Result()

    def thread(result, updated):
        result.result = i.get_and_set(updated)

    obj_0 = 500
    id_0 = id(obj_0)
    obj_1 = 600
    id_1 = id(obj_1)
    t0 = threading.Thread(
        target=thread,
        args=(
            result_0,
            obj_0,
        ),
    )
    t1 = threading.Thread(
        target=thread,
        args=(
            result_1,
            obj_1,
        ),
    )
    t0.start()
    t1.start()
    t0.join()
    t1.join()
    results = [result_0.result, result_1.result]

    assert 400 in results
    results.remove(400)
    assert results[0] in [obj_0, obj_1]
    assert id(obj_0) == id_0 and id(obj_1) == id_1


def test_increment_and_get():
    assert AtomicInt(0).increment_and_get(1) == 1
    assert AtomicInt(0).increment_and_get(None) == 1
    assert AtomicInt(0).increment_and_get() == 1


def test_get_and_increment():
    i = AtomicInt(0)
    assert i.get_and_increment(1) == 0
    assert i.get() == 1
    i = AtomicInt(0)
    assert i.get_and_increment(None) == 0
    assert i.get() == 1
    i = AtomicInt(0)
    assert i.get_and_increment() == 0
    assert i.get() == 1


def test_decrement_and_get():
    assert AtomicInt(0).decrement_and_get(1) == -1
    assert AtomicInt(0).decrement_and_get(None) == -1
    assert AtomicInt(0).decrement_and_get() == -1


def test_get_and_decrement():
    i = AtomicInt(0)
    assert i.get_and_decrement(1) == 0
    assert i.get() == -1
    i = AtomicInt(0)
    assert i.get_and_decrement(None) == 0
    assert i.get() == -1
    i = AtomicInt(0)
    assert i.get_and_decrement() == 0
    assert i.get() == -1


def test_get_and_update():
    i = AtomicInt(0)
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 0
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 1
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 2
    assert i.get_and_update(lambda _: (_ + 1) % 3) == 0

    with raises(TypeError):
        i.get_and_update(lambda _, mmm: _ + mmm)


def test_update_and_get():
    i = AtomicInt(0)
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 1
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 2
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 0
    assert i.update_and_get(lambda _: (_ + 1) % 3) == 1

    with raises(TypeError):
        i.update_and_get(lambda _, mmm: _ + mmm)


def test_get_handle():
    i = AtomicInt()
    h = i.get_handle()
    assert isinstance(h, cereggii.AtomicIntHandle)
    assert isinstance(h.get_handle(), cereggii.AtomicIntHandle)


def test_dealloc():
    AtomicInt()
    previous = None
    while (this := gc.collect()) != previous:
        previous = this


def test_hash():
    i = AtomicInt(0)
    assert type(hash(i)) == int  # noqa: E721
    assert hash(i) != hash(0)
    h = i.get_handle()
    hh = h.get_handle()
    assert hash(i) == hash(h) == hash(hh)


def test_eq():
    zero = AtomicInt(0)
    assert zero == 0
    assert zero == 0.0
    assert zero == Fraction(0, 1)
    assert zero == (0 + 0j)


def test_add():
    identity = AtomicInt(0)
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
    identity = AtomicInt(0)
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
    identity = AtomicInt(1)
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
    assert AtomicInt(100) % 9 == 1


def test_divmod():
    assert divmod(AtomicInt(100), 9) == (11, 1)


def test_power():
    assert AtomicInt(2) ** 2 == 4


def test_negative():
    assert -AtomicInt(1) == -1
    assert -AtomicInt(-1) == 1
    assert -AtomicInt(0) == 0


def test_positive():
    assert +AtomicInt(1) == 1
    assert +AtomicInt(-1) == -1
    assert +AtomicInt(0) == 0


def test_absolute():
    assert abs(AtomicInt(-1)) == 1
    assert abs(AtomicInt(1)) == 1
    assert abs(AtomicInt(0)) == 0


def test_bool():
    i = bool(AtomicInt(100))
    assert i
    assert type(i) == bool  # noqa: E721

    i = bool(AtomicInt(0))
    assert not i
    assert type(i) == bool  # noqa: E721


def test_invert():
    assert -AtomicInt(1) == -1


def test_lshift():
    assert AtomicInt(1) << 64 == 1 << 64


def test_rshift():
    assert AtomicInt(1) >> 1 == 0


def test_and():
    assert AtomicInt(128) & 128 == 128
    assert AtomicInt(128) & 64 == 0


def test_xor():
    assert AtomicInt(128) ^ 128 == 0
    assert AtomicInt(128) ^ 64 == 128 + 64


def test_or():
    assert AtomicInt(128) | 128 == 128
    assert AtomicInt(128) | 64 == 128 + 64


def test_int():
    i = int(AtomicInt(100))
    assert i == 100
    assert type(i) == int  # noqa: E721


def test_float():
    f = float(AtomicInt(100))
    assert f == 100.0
    assert type(f) == float  # noqa: E721


def test_iadd():
    i = AtomicInt(0)
    i += 2**62
    assert i.get() == 2**62

    with raises(OverflowError):
        i += 2**62


def test_isub():
    i = AtomicInt(0)
    i -= 2**62
    assert i.get() == -(2**62)

    with raises(OverflowError):
        i -= 2**62 + 1


def test_imul():
    i = AtomicInt(1)
    i *= 2**62
    assert i.get() == 2**62

    with raises(OverflowError):
        i *= 2


def test_iremainder():
    i = AtomicInt(100)
    i %= 9
    assert i == 1


def test_ipower():
    i = AtomicInt(100)
    i **= 2
    assert i == 10000


def test_ilshift():
    i = AtomicInt(100)
    i <<= 9
    assert i == 51200


def test_irshift():
    i = AtomicInt(100)
    i >>= 9
    assert i == 0


def test_iand():
    i = AtomicInt(100)
    i &= 9
    assert i == 0


def test_ixor():
    i = AtomicInt(100)
    i ^= 9
    assert i == 109


def test_ior():
    i = AtomicInt(100)
    i |= 9
    assert i == 109


def test_inplace_floor_divide():
    i = AtomicInt(500)
    i //= 10
    assert i == 50


def test_inplace_true_divide():
    i = AtomicInt()
    with raises(NotImplementedError):
        i /= 1


def test_floor_divide():
    result = AtomicInt(500) // 10
    assert result == 50
    assert type(result) == int  # noqa: E721


def test_true_divide():
    result = AtomicInt(500) / 10
    assert result == 50
    assert type(result) == float  # noqa: E721


def test_index():
    i = AtomicInt(0)
    assert i.__index__() == 0


def test_as_integer_ratio():
    with raises(NotImplementedError):
        AtomicInt().as_integer_ratio()


def test_bit_length():
    with raises(NotImplementedError):
        AtomicInt().bit_length()


def test_conjugate():
    with raises(NotImplementedError):
        AtomicInt().conjugate()


def test_from_bytes():
    with raises(NotImplementedError):
        AtomicInt().from_bytes()


def test_to_bytes():
    with raises(NotImplementedError):
        AtomicInt().to_bytes()


def test_denominator():
    with raises(NotImplementedError):
        AtomicInt().denominator  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt().denominator.__set__(0)


def test_numerator():
    with raises(NotImplementedError):
        AtomicInt().numerator  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt().numerator.__set__(0)


def test_imag():
    with raises(NotImplementedError):
        AtomicInt().imag  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt().imag.__set__(0)


def test_real():
    with raises(NotImplementedError):
        AtomicInt().real  # noqa: B018
    with raises(NotImplementedError):
        AtomicInt().real.__set__(0)
