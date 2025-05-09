# deque

The next-generation deque container for C++, written and designed using modern C++ with sufficiently good performance. Fully maintaining compatibility with the std::deque and supports contiguous access through buckets. Requires C++23.

Working in progress.

Additionally, [the noalloc branch](https://github.com/YexuanXiao/deque/tree/noalloc) provides an allocator-free version to explain the design of the deque and facilitate further modifications. The repository [deque-test](https://github.com/yexuanXiao/deque-test) provides a ported version of the test cases from libc++.

## Roadmap

+ performance benchmark

## Compiler Portability

GCC 11+, Clang 13+, MSVC 19.43+

## Standard Library Portability

libstdc++10+, libc++16+, MSVC STL 19.29+

## Notes

Build tests can currently only using the STL, due to libc++ has not yet implemented `views::enumerate` and libstdc++ lacks `from_ranges` constructors.
