# deque

The next-generation deque container for C++, written and designed using modern C++ with sufficiently good performance. Fully maintaining compatibility with the std::deque and supports contiguous access through buckets. Requires C++20.

The [noalloc](https://github.com/YexuanXiao/deque/tree/noalloc) branch provides an allocator-free version to explain the design of the deque and facilitate further modifications. The repository [deque-test](https://github.com/YexuanXiao/deque-test) provides a ported version of the test cases from libc++. The repository [deque-benchmark](https://github.com/YexuanXiao/deque-benchmark) provides performance tests to compare the performance differences between this and the three other standard library implementations.

## Compiler Portability

GCC 11+, Clang 13+, MSVC 19.43+

## Standard Library Portability

libstdc++10+, libc++16+, MSVC STL 19.29+

## Notes

It is recommended to use the latest compilers and standard libraries as they provide more optimized code.

Build tests cannot using the libc++, due to libc++ has not yet implemented `views::enumerate`.
