# deque

The next-generation deque container for C++, written and designed using modern C++ with sufficiently good performance. Fully maintaining compatibility with the std::deque and supports contiguous access through buckets. Requires C++23.

Working in progress.

## Roadmap

+ `insert`
+ `insert_range`
+ `operator==`
+ `operator<=>`
+ `erase_if`
+ allocator support
+ performance benchmark
+ tests of the above functions and bucket and iterator

## Compiler Portability

GCC 11+, Clang 13+, MSVC 19.43+

## Standard Library Portability

libstdc++10+, libc++16+, MSVC STL 19.29+

## Notes

Build tests can currently only using the STL, due to libc++ has not yet implemented `views::enumerate` and libstdc++ lacks `from_ranges` constructors.
