# deque

The next-generation deque container for C++, written and designed using modern C++ with sufficiently good performance. Fully maintaining compatibility with the std::deque and supports contiguous access through buckets. Requires C++23.

Working in progress.

This branch provides an allocator-free version, making the code easier to read and modify.

## Compiler Portability

GCC 11+, Clang 13+, MSVC 19.43+

## Standard Library Portability

libstdc++10+, libc++16+, MSVC STL 19.29+

## Notes

It is recommended to use the latest compilers and standard libraries as they provide more optimized code.

Build tests cannot using the libc++, due to libc++ has not yet implemented `views::enumerate`.
