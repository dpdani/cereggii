# Examples

This section provides practical examples demonstrating how to use cereggii's concurrent data structures effectively in real-world scenarios.

## AtomicDict Examples

Learn how to leverage the power of lock-free dictionaries for high-performance concurrent programming.

### [Using `reduce()` for Multithreaded Aggregation](AtomicDict/reduce.md)

Learn the fundamentals of concurrent data aggregation with `AtomicDict`. This example demonstrates:

- How to use [`reduce()`][cereggii._cereggii.AtomicDict.reduce] for thread-safe aggregation
- Performance comparison between single-threaded `dict` and multithreaded `AtomicDict`
- The benefits of specialized reduction functions like [`reduce_sum()`][cereggii._cereggii.AtomicDict.reduce_sum]
- Achieving linear speedup with multiple threads

**Key concepts:** Thread-safe aggregation, compare-and-set operations, specialized reduce functions

### [Using `reduce()` for Averaging](AtomicDict/reduce-average.md)

A more advanced example showing how to handle operations that aren't naturally commutative or associative. This example covers:

- Transforming non-commutative operations into multithreading-friendly ones
- Handling implicit contention with [`ThreadHandle`][cereggii._cereggii.ThreadHandle]
- Performance optimization techniques for free-threading Python
- Computing averages correctly across multiple threads

**Key concepts:** Commutative operations, thread-local handles, avoiding shared object contention

## Getting Started

If you're new to concurrent programming with cereggii, we recommend:

1. Start with the [reduce() example](AtomicDict/reduce.md) to understand the basics
2. Progress to the [averaging example](AtomicDict/reduce-average.md) for advanced techniques
3. Explore the [API Reference](../api/index.md) for detailed documentation

## Contributing Examples

Have a great example demonstrating cereggii's capabilities? Contributions are welcome! Visit the [cereggii repository](https://github.com/dpdani/cereggii) to submit your examples.
