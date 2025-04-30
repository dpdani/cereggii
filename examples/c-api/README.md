# Using cereggii's C-APIs

This examples showcases how to use cereggii's C-level APIs, highlighting how to
correctly specify it as a build-time dependency.

The problems:

1. we need to be able to compile the program using cereggii's headers.
2. we may additionally need to link the pre-built shared library to use the C-APIs
at runtime.
   - you don't need to link against the shared library, if you only need the header-only atomics.
