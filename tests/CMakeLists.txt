enable_language(CXX)
find_package(Threads REQUIRED)

set(SHMIPC_PUBLIC_INCLUDE_DIR   "${CMAKE_SOURCE_DIR}/include")
set(SHMIPC_INTERNAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src")

include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.12
)
FetchContent_MakeAvailable(doctest)

function(target_test_flags tgt)
  target_compile_options(${tgt} PRIVATE -O0 -g -UNDEBUG) 
  target_enable_warnings(${tgt})
  target_compile_options(${tgt} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>
  )
  target_fix_posix(${tgt})
endfunction()

function(add_shmipc_test name)
  add_executable(${name} ${ARGN})
  
  target_include_directories(${name} PRIVATE
    ${SHMIPC_PUBLIC_INCLUDE_DIR}
    ${SHMIPC_INTERNAL_INCLUDE_DIR}
    ${doctest_SOURCE_DIR}
  )
  
  target_link_libraries(${name} PRIVATE
    doctest::doctest
    shmipc_static
    Threads::Threads
  )
  
  target_test_flags(${name})
  
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_shmipc_test(ipc_buffer_basic_test         ipc_buffer_basic_test.cpp)
add_shmipc_test(ipc_channel_basic_test        ipc_channel_basic_test.cpp)
add_shmipc_test(ipc_mmap_test                 ipc_mmap_test.cpp)

add_shmipc_test(ipc_buffer_concurrency_test   ipc_buffer_concurrency_test.cpp)
add_shmipc_test(ipc_channel_concurrent_test   ipc_channel_concurrency_test.cpp)

add_shmipc_test(ipc_channel_comprehensive_test ipc_channel_comprehensive_test.cpp)

# Performance and stress tests are excluded from CI - run manually via scripts/run_performance_tests.sh
# if(NOT DEFINED ENV{CI})
#     add_shmipc_test(ipc_channel_performance_test ipc_channel_performance_test.cpp)
#     add_shmipc_test(ipc_buffer_stress_test        ipc_buffer_stress_test.cpp)
#     add_shmipc_test(ipc_channel_stress_test      ipc_channel_stress_test.cpp)
# endif()