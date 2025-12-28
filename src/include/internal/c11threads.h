// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_C11THREADS_H
#define CEREGGII_C11THREADS_H

// macOS doesn't have threads.h >:/

#if defined(__STDC_NO_THREADS__) || defined(__APPLE__) || (defined(__GNUC__) && !defined(__clang__) && __STDC_VERSION__ < 201112L)
// Fallback for systems without <threads.h> (like macOS or older GLIBC)
// We use pthreads to implement the subset of C11 threads we need

#include <pthread.h>
#include <time.h>
#include <errno.h>

typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_t thrd_t;

enum {
    thrd_success = 0,
    thrd_error = 1,
    thrd_nomem = 2,
    thrd_busy = 3,
    thrd_timedout = 4
};

enum {
    mtx_plain = 0,
    mtx_recursive = 1,
    mtx_timed = 2
};

static inline int mtx_init(mtx_t *mtx, int type) {
    if (type == mtx_plain) {
        if (pthread_mutex_init(mtx, NULL) == 0) return thrd_success;
    } else if (type == mtx_recursive) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        int res = pthread_mutex_init(mtx, &attr);
        pthread_mutexattr_destroy(&attr);
        if (res == 0) return thrd_success;
    }
    return thrd_error;
}

static inline void mtx_destroy(mtx_t *mtx) {
    pthread_mutex_destroy(mtx);
}

static inline int mtx_lock(mtx_t *mtx) {
    if (pthread_mutex_lock(mtx) == 0) return thrd_success;
    return thrd_error;
}

static inline int mtx_unlock(mtx_t *mtx) {
    if (pthread_mutex_unlock(mtx) == 0) return thrd_success;
    return thrd_error;
}

static inline int cnd_init(cnd_t *cond) {
    if (pthread_cond_init(cond, NULL) == 0) return thrd_success;
    return thrd_error;
}

static inline void cnd_destroy(cnd_t *cond) {
    pthread_cond_destroy(cond);
}

static inline int cnd_wait(cnd_t *cond, mtx_t *mtx) {
    if (pthread_cond_wait(cond, mtx) == 0) return thrd_success;
    return thrd_error;
}

static inline int cnd_broadcast(cnd_t *cond) {
    if (pthread_cond_broadcast(cond) == 0) return thrd_success;
    return thrd_error;
}

static inline int cnd_signal(cnd_t *cond) {
    if (pthread_cond_signal(cond) == 0) return thrd_success;
    return thrd_error;
}

#else
#include <threads.h>
#endif

#endif //CEREGGII_C11THREADS_H
