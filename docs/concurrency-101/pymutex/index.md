# Recreating PyMutex

In this chapter, we'll try to recreate Sam Gross' PyMutex lock, using the
utilities that cereggii provides. This particular lock is very performant, and
understanding where the performance is gained will help you understand how to
gain performance in your own programs.

We'll be writing our PyMutex in Python code, but what you'll see here mostly
applies to C as well. In C, you'll have to also take care of correctly following
the constraints of the memory model; in Python we don't have to.

This is a multipart journey, we'll begin with reviewing the performance of two
standard locks, and then we'll delve into the details, choices, and tradeoffs of
PyMutex.

- [Part 1 — TAS vs TTAS](./test-and-set.md)
- ...
