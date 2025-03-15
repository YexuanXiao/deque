cmake_minimum_required(VERSION 3.10)

project(deque)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(test_func test_func.cpp)
add_executable(test_consis test_consis.cpp)

add_test(NAME test_functional COMMAND test_func)
add_test(NAME test_consistency COMMAND test_consis)
