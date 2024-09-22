# Safety

When multiple threads can mutate (read and write to) the same piece of memory,
they need to be synchronized to avoid undefined behavior.

> [https://xkcd.com/2200/](https://xkcd.com/2200/)


## Mutual Exclusion
### Reentrancy
