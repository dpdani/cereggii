#include "Python.h"

#ifdef Py_GIL_DISABLED

// The shared reference count uses the two least-significant bits to store
// flags. The remaining bits are used to store the reference count.
#define _Py_REF_SHARED_SHIFT        2
#define _Py_REF_SHARED_FLAG_MASK    0x3

// The shared flags are initialized to zero.
#define _Py_REF_SHARED_INIT         0x0
#define _Py_REF_MAYBE_WEAKREF       0x1
#define _Py_REF_QUEUED              0x2
#define _Py_REF_MERGED              0x3


/* Tries to increment an object's reference count
 *
 * This is a specialized version of _Py_TryIncref that only succeeds if the
 * object is immortal or local to this thread. It does not handle the case
 * where the  reference count modification requires an atomic operation. This
 * allows call sites to specialize for the immortal/local case.
 */
static inline int
_Py_TryIncrefFast(PyObject *op) {
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    local += 1;
    if (local == 0) {
        // immortal
        return 1;
    }
    if (_Py_IsOwnedByCurrentThread(op)) {
        _Py_INCREF_STAT_INC();
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);
//#ifdef Py_REF_DEBUG
//        _Py_IncRefTotal(_PyThreadState_GET());
//#endif
        return 1;
    }
    return 0;
}

static inline int
_Py_TryIncRefShared(PyObject *op)
{
    Py_ssize_t shared = _Py_atomic_load_ssize_relaxed(&op->ob_ref_shared);
    for (;;) {
        // If the shared refcount is zero and the object is either merged
        // or may not have weak references, then we cannot incref it.
        if (shared == 0 || shared == _Py_REF_MERGED) {
            return 0;
        }

        if (_Py_atomic_compare_exchange_ssize(
                &op->ob_ref_shared,
                &shared,
                shared + (1 << _Py_REF_SHARED_SHIFT))) {
//#ifdef Py_REF_DEBUG
//            _Py_IncRefTotal(_PyThreadState_GET());
//#endif
            _Py_INCREF_STAT_INC();
            return 1;
        }
    }
}

/* Tries to incref the object op and ensures that *src still points to it. */
static inline int
_Py_TryIncrefCompare(PyObject **src, PyObject *op)
{
    if (_Py_TryIncrefFast(op)) {
        return 1;
    }
    if (!_Py_TryIncRefShared(op)) {
        return 0;
    }
    if (op != _Py_atomic_load_ptr(src)) {
        Py_DECREF(op);
        return 0;
    }
    return 1;
}

/* Loads and increfs an object from ptr, which may contain a NULL value.
   Safe with concurrent (atomic) updates to ptr.
   NOTE: The writer must set maybe-weakref on the stored object! */
static inline PyObject *
_Py_XGetRef(PyObject **ptr)
{
    for (;;) {
        PyObject *value = _Py_atomic_load_ptr(ptr);
        if (value == NULL) {
            return value;
        }
        if (_Py_TryIncrefCompare(ptr, value)) {
            return value;
        }
    }
}

/* Attempts to loads and increfs an object from ptr. Returns NULL
   on failure, which may be due to a NULL value or a concurrent update. */
static inline PyObject *
_Py_TryXGetRef(PyObject **ptr)
{
    PyObject *value = _Py_atomic_load_ptr(ptr);
    if (value == NULL) {
        return value;
    }
    if (_Py_TryIncrefCompare(ptr, value)) {
        return value;
    }
    return NULL;
}

/* Like Py_NewRef but also optimistically sets _Py_REF_MAYBE_WEAKREF
   on objects owned by a different thread. */
static inline PyObject *
_Py_NewRefWithLock(PyObject *op)
{
    if (_Py_TryIncrefFast(op)) {
        return op;
    }
//#ifdef Py_REF_DEBUG
//    _Py_IncRefTotal(_PyThreadState_GET());
//#endif
    _Py_INCREF_STAT_INC();
    for (;;) {
        Py_ssize_t shared = _Py_atomic_load_ssize_relaxed(&op->ob_ref_shared);
        Py_ssize_t new_shared = shared + (1 << _Py_REF_SHARED_SHIFT);
        if ((shared & _Py_REF_SHARED_FLAG_MASK) == 0) {
            new_shared |= _Py_REF_MAYBE_WEAKREF;
        }
        if (_Py_atomic_compare_exchange_ssize(
                &op->ob_ref_shared,
                &shared,
                new_shared)) {
            return op;
        }
    }
}

static inline PyObject *
_Py_XNewRefWithLock(PyObject *obj)
{
    if (obj == NULL) {
        return NULL;
    }
    return _Py_NewRefWithLock(obj);
}

static inline void
_PyObject_SetMaybeWeakref(PyObject *op)
{
    if (_Py_IsImmortal(op)) {
        return;
    }
    for (;;) {
        Py_ssize_t shared = _Py_atomic_load_ssize_relaxed(&op->ob_ref_shared);
        if ((shared & _Py_REF_SHARED_FLAG_MASK) != 0) {
            // Nothing to do if it's in WEAKREFS, QUEUED, or MERGED states.
            return;
        }
        if (_Py_atomic_compare_exchange_ssize(
                &op->ob_ref_shared, &shared, shared | _Py_REF_MAYBE_WEAKREF)) {
            return;
        }
    }
}

#else

static inline void
_PyObject_SetMaybeWeakref(PyObject *op) {}


#endif

/* Tries to incref op and returns 1 if successful or 0 otherwise. */
static inline int
_Py_TryIncref(PyObject *op)
{
#ifdef Py_GIL_DISABLED
    return _Py_TryIncrefFast(op) || _Py_TryIncRefShared(op);
#else
    if (Py_REFCNT(op) > 0) {
        Py_INCREF(op);
        return 1;
    }
    return 0;
#endif
}
