cmake_minimum_required(VERSION 3.14)
project(ipc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Источники
file(GLOB_RECURSE SRC_FILES src/*.c)
add_library(ipc_lib ${SRC_FILES})
target_include_directories(ipc_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Enable testing
include(CTest)
enable_testing()

# Criterion (вручную через pkg-config)
find_library(CRITERION_LIBRARY criterion REQUIRED)
find_path(CRITERION_INCLUDE_DIR criterion/criterion.h REQUIRED)

# Исполняемый файл тестов
file(GLOB_RECURSE TEST_FILES tests/*.c)
add_executable(ipc_tests ${TEST_FILES})
target_include_directories(ipc_tests PRIVATE ${CRITERION_INCLUDE_DIR})
target_link_libraries(ipc_tests PRIVATE ipc_lib ${CRITERION_LIBRARY})

# Регистрируем как CTest-тест
add_test(NAME all_tests COMMAND ipc_tests)

