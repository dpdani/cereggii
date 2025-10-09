# cereggii

[![supports free-threading](https://github.com/dpdani/free-threading-badges/raw/main/supported.svg)](https://py-free-threading.github.io/)
[![PyPI - Version](https://img.shields.io/pypi/v/cereggii?color=blue)](https://pypi.org/project/cereggii/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/cereggii)](https://pypi.org/project/cereggii/)
[![GitHub License](https://img.shields.io/github/license/dpdani/cereggii)](https://github.com/dpdani/cereggii/blob/main/LICENSE)

Thread synchronization utilities for Python.

This library is beta quality: expect things to change.

[Documentation is here.](https://dpdani.github.io/cereggii)

```python
from cereggii import AtomicDict, ThreadSet

counter = AtomicDict({"red": 42, "green": 3, "blue": 14})

@ThreadSet.repeat(10)  # create 10 threads
def workers():
    counter.reduce_count(["red"] * 60 + ["green"] * 7 + ["blue"] * 30)

workers.start_and_join()

assert counter["red"] == 642
assert counter["green"] == 73
assert counter["blue"] == 314
```


## Installation

The recommended installation method is to download binary wheels from PyPI:

```shell
pip install cereggii
```
```shell
uv add cereggii
```

### Installing from sources

To install from source, first pull the repository:

```shell
git clone https://github.com/dpdani/cereggii
```

Then, install the build requirements (do this in a virtualenv):

```shell
pip install -e .[dev]
```

Finally, run the tests:

```shell
pytest
```

## License

Apache License 2.0. See the [LICENSE](LICENSE) file.


## Links

- Documentation: https://dpdani.github.io/cereggii/
- Source: https://github.com/dpdani/cereggii
- Issues: https://github.com/dpdani/cereggii/issues


## Cereus greggii

<img src="https://raw.githubusercontent.com/dpdani/cereggii/refs/heads/main/.github/cereggii.jpg" align="right">

The *Peniocereus Greggii* (also known as *Cereus Greggii*) is a flower native to
Arizona, New Mexico, Texas, and some parts of northern Mexico.

This flower blooms just one summer night every year and in any given area, all
these flowers bloom in synchrony.

[Wikipedia](https://en.wikipedia.org/wiki/Peniocereus_greggii)

_Image credits: Patrick Alexander, Peniocereus greggii var. greggii, south of
Cooke's Range, Luna County, New Mexico, 10 May 2018, CC0.
[source](https://www.flickr.com/photos/aspidoscelis/42926986382)_
