# Performance

## It's all about the cache

Understanding the performance of a concurrent program is very different from
that of a sequential program. Let's turn to C here: the relevant subtleties are
much more easily appreciated in a lower-level language.

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

    1. These `memory_order_*` shenanigans are due to C's memory model. You don't
       need to know exactly what they mean to follow this section. If you're curious
       you can refer to [this](./low-safety.md#memory-models).

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

You can easily see that the two programs are executing the same instructions.
The only difference is the address the threads execute their routine on. If this
was a sequential program, you would expect them to have the same running time.
Instead, the "Slow" program takes ~2.5s, while the "Fast" program takes ~0.5s.

Why?


## The Implementation of CAS

A compare-and-set instruction (see [here](./low-safety.md#compare-and-set) for
an introduction) is usually implemented by hardware in cooperation with the
cache sub-system. (Other atomic instructions are implemented similarly.)

!!! info

    The hardware cache is usually divided into pieces of 64 or 128 bytes called
    *lines*. When a memory address is read from main memory into the cache, an
    entire line is loaded into the cache of the core that requested it.

It is important to note that in recent architectures the cache itself is a
hierarchy of smaller caches, usually divided into L1, L2, and L3 layers. L1 is
the "closest" to the core, it is the fastest and the smallest. L3 is the "farthest"
from the core, it is the slowest and the largest.

An L1 layer of cache is usually assigned specifically to one core, an L2 layer
to a subset of cores, and an L3 to a larger subset still.

Therefore, not all cores have access to the same memory. For concurrent
shared-memory programs, it means that several cores may refer to the same
logical locations of memory from different physical locations.

The CPU itself is assigned the task of keeping all the pieces of cache coherent
with one another.
When several cores share a line of cache and one core modifies it, the hardware has
to make sure that the remaining cores eventually see the modification.
(Also see [Wikipedia's article on cache coherence](https://en.wikipedia.org/wiki/Cache_coherence).)

In such a scenario, when one core modifies a shared cache line with an atomic
instruction, before it succeeds in its modification it has to ensure that it
owns the most recent view of that cache line. When it's certain of that, the
check against the expected value is performed.

Later, if other cores want to also modify the same line with an atomic
instruction, they need to make sure to have the most recent view, too.

This behavior can generate a lot of traffic in the memory bus of the CPU, when a
line of cache is contended among cores.

### False Sharing

A corollary to the implementation of CAS is known as False Sharing. 

When contention occurs, one would expect it only to pertain to a single memory
location, but it's not so. It pertains to the entire _cache line_ being contended.

Thus, distinct pieces of memory that happen to reside in the same line of cache
are _falsely shared_ (and contended), between the CPU cores, triggering traffic
in the memory bus.


## Five rules of thumb

There are five practical rules that can help you improve the performance of your
program. These are drawn from the excellent book "The Art of Multiprocessor
Programming" by M. Herlihy and N. Shavit.

> - Objects or fields that are accessed independently should be aligned and
    padded so that they end up on different cache lines.

This is the simplest solution to false sharing: pad your data structures, use
more memory, and ensure that objects end up on different lines of cache.

You can see an application of this rule in the "Fast" variant of the program
above.

!!! tip

    Often a line of cache is 64 bytes wide. You can check the line size in your
    system by running `getconf LEVEL1_DCACHE_LINESIZE`.

> - Keep read-only data separate from data that is modified frequently.
>  
>   For example, consider a list whose structure is constant, but whose elements' value
    fields change frequently. To ensure that modifications do not slow down list
    traversals, one could align and pad the value fields so that each one fills up a
    cache line.

The example is exactly how a Python list works: one array of pointers to
`PyObject`s that reside in other locations.

> - When possible, split an object into thread-local pieces.
>
>   For example, a
    counter used for statistics could be split up into an array of counters, one per
    thread, each one residing on a different cache line. While a shared counter
    would cause invalidation traffic, the split counter allows each thread to update
    its own replica without causing coherence traffic.

[//]: # (!!! tip)

[//]: # (    That's [AtomicIntCounter][cereggii._cereggii.AtomicIntCounter]!)
TODO


> - If a lock protects data that is frequently modified, then keep the lock and
    the data on distinct cache lines, so that threads trying to acquire the lock do
    not interfere with the lock holder's access to the data.

TODO

> - If a lock protects data that is frequently uncontended, then try to keep the
    lock and the data on the same cache lines, so that acquiring the lock will also
    load some of the data into the cache.
