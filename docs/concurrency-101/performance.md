# Performance

## It's all about the cache

Understanding the performance of a concurrent program is very different from
that of a sequential program. To begin this discussion let us turn to C, for
the relevant microscopic subtleties are much more easily appreciated.

!!! note

    Here, we'll use `pthreads.h` and `stdatomic.h`. If you're unfamiliar with them,
    you should still be able to follow along. If you want to also execute this code,
    be sure to check whether you have these available for your platform, or rewrite
    the programs with what's available for you. These two libraries are generally
    available in recent Linux environments.

Let's compare these simple C programs, where two threads execute the
`thread_routine` function in parallel:

=== "Slow"

    ```c
    #include <stdlib.h>
    #include <pthread.h>
    #include <stdatomic.h>
    
    
    void *thread_routine(void *spam) {
        int8_t expected = 0, desired = 0;
    
        for (int i = 0; i < 100000000UL; i++) {
            atomic_compare_exchange_strong_explicit(
                (int8_t *) spam, &expected, desired,
                memory_order_acq_rel, memory_order_acquire // (1)
            );
        }
    
        return spam;
    }
    
    int main(void) {
        pthread_t thread1, thread2;
        int8_t* spam = malloc(1);
    
        int t1 = pthread_create(&thread1, NULL, thread_routine, (void *) spam);
        int t2 = pthread_create(&thread2, NULL, thread_routine, (void *) spam);
    
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
    
        return t1 + t2;
    }
    ```

    1. These `memory_order_*` shenanigans concern C's memory model. You don't
       need to know exactly what they mean to follow this section. If you're curious
       you can refer to the Memory Models section.

=== "Fast"

    ```{ .c .annotate }
    #include <stdlib.h>
    #include <pthread.h>
    #include <stdatomic.h>
    
    
    void *thread_routine(void *spam) {
        int8_t expected = 0, desired = 0;
    
        for (int i = 0; i < 100000000UL; i++) {
            atomic_compare_exchange_strong_explicit(
                (int8_t *) spam, &expected, desired,
                memory_order_acq_rel, memory_order_acquire
            );
        }
    
        return spam;
    }
    
    int main(void) {
        pthread_t thread1, thread2;
        int8_t* spam = malloc(64 + 1);  // (1)
    
        int t1 = pthread_create(&thread1, NULL, thread_routine, (void *) spam);
        int t2 = pthread_create(&thread2, NULL, thread_routine, (void *) (spam + 64));
    
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
    
        return t1 + t2;
    }
    ```

    1. Allocate the array in two distinct cache lines.
       Often a line of cache is 64 bytes wide. You can check the line size in your
       system by running `getconf LEVEL1_DCACHE_LINESIZE`.

You can easily see that the two programs are executing the same number of the
same kinds of instructions. The only difference is the address the threads
execute their routine on. If this was a sequential program, you would expect
them to have the same running time. Instead, the "Slow" program takes ~2.5s,
while the "Fast" program takes ~0.5s.

Why?

## Five rules of thumb

There are five very practical rules that can help you improve the performance of
your program. These are drawn from the excellent book "The Art of Multiprocessor
Programming" by M. Herlihy and N. Shavit.

> - Objects or fields that are accessed independently should be aligned and
    padded so that they end up on different cache lines.

asd

> - Keep read-only data separate from data that is modified frequently. For
    example, consider a list whose structure is constant, but whose elements' value
    fields change frequently. To ensure that modifications do not slow down list
    traversals, one could align and pad the value fields so that each one fills up a
    cache line.

asd

> - When possible, split an object into thread-local pieces. For example, a
    counter used for statistics could be split up into an array of counters, one per
    thread, each one residing on a different cache line. While a shared counter
    would cause invalidation traffic, the split counter allows each thread to update
    its own replica without causing coherence traffic.

asd

> - If a lock protects data that is frequently modified, then keep the lock and
    the data on distinct cache lines, so that threads trying to acquire the lock do
    not interfere with the lock holder's access to the data.

asd

> - If a lock protects data that is frequently uncontended, then try to keep the
    lock and the data on the same cache lines, so that acquiring the lock will also
    load some of the data into the cache.
