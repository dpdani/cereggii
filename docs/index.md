# cereggii

> Thread synchronization utilities for Python

This package provides atomic versions of common data structures:

- [AtomicDict][cereggii._cereggii.AtomicDict]
- [AtomicInt][cereggii._cereggii.AtomicInt]
- [AtomicRef][cereggii._cereggii.AtomicRef]
- …and more to come

If you are novel to concurrent programming, you can read the [Concurrency
101](./concurrency-101/index.md) guide.

[//]: # (Along with the Python bindings, you can also find equivalent [C-APIs]&#40;./c-api/...&#41;.)


## Installing

*Arm disclaimer:* `aarch64` processors are generally not supported, but this
library was successfully used with Apple Silicon.

Using [@colesbury's original nogil
fork](https://github.com/colesbury/nogil?tab=readme-ov-file#installation) is
required to use this library. You can get it with pyenv:

```shell
pyenv install nogil-3.9.10-1
```

Then, you may fetch this library [from PyPI](https://pypi.org/project/cereggii):

```shell
pip install cereggii
```

## Experimental

This library is experimental and should not be used in a production environment.

After all, as of now, it requires a non-official fork in order to run.

Porting to free-threaded Python 3.13 (3.13t) is planned.
