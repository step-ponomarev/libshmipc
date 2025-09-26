enable_language(CXX)
find_package(Threads REQUIRED)

set(SHMIPC_PUBLIC_INCLUDE_DIR   "${CMAKE_SOURCE_DIR}/include")
set(SHMIPC_INTERNAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src")   # приватные .h из src/

function(target_relax_test_warnings tgt)
  target_compile_options(${tgt} PRIVATE -w)
 
  target_compile_options(${tgt} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>
  )
endfunction()

add_library(ipc_test STATIC test_runner.c)
target_include_directories(ipc_test PRIVATE
  ${SHMIPC_PUBLIC_INCLUDE_DIR}
  ${SHMIPC_INTERNAL_INCLUDE_DIR}
)
target_relax_test_warnings(ipc_test)

function(add_shmipc_test name)
  add_executable(${name} ${ARGN})

  target_include_directories(${name} PRIVATE
    ${SHMIPC_PUBLIC_INCLUDE_DIR}
    ${SHMIPC_INTERNAL_INCLUDE_DIR}
  )

  target_relax_test_warnings(${name})

  target_link_libraries(${name} PRIVATE
    shmipc_static
    ipc_test
    Threads::Threads
  )

  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_shmipc_test(ipc_buffer_basic_test         ipc_buffer_basic_test.c)
add_shmipc_test(ipc_buffer_concurrency_test   ipc_buffer_concurrency_test.cpp)
add_shmipc_test(ipc_channel_basic_test        ipc_channel_basic_test.c)
add_shmipc_test(ipc_channel_concurrent_test   ipc_channel_concurrency_test.cpp)
add_shmipc_test(ipc_mmap_test                 ipc_mmap_test.c)
