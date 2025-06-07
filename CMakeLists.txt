cmake_minimum_required(VERSION 3.14)
project(libshmipc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)

