cmake_minimum_required(VERSION 3.10)

project(deque)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(tests tests.cpp)

add_test(NAME Test COMMAND tests)