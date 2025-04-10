cmake_minimum_required(VERSION 3.10)

project(deque)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(test_functional tests.cpp)
target_compile_definitions(test_functional PRIVATE TEST_FUNC)
add_executable(test_consistency tests.cpp)
target_compile_definitions(test_consistency PRIVATE TEST_CONSIS)

add_test(NAME test_functional COMMAND test_functional)
add_test(NAME test_consistency COMMAND test_consistency)
