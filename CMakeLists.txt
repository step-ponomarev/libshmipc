cmake_minimum_required(VERSION 3.14)
project(IPCProject C)

set(CMAKE_C_STANDARD 11)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)

