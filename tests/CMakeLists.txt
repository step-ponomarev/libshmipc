# Тесты завязаны на C и C++
enable_language(CXX)

# Если нужны потоки:
find_package(Threads REQUIRED)

# Общая статическая либка с хелперами тестов
add_library(ipc_test STATIC test_runner.c)
target_enable_warnings(ipc_test)

# Функция линковки теста с библиотекой и потоками
function(add_shmipc_test name)
  add_executable(${name} ${ARGN})
  target_enable_warnings(${name})

  # Линкуем с той сборкой, что доступна (shared предпочтительнее)
  if (TARGET shmipc_shared)
    target_link_libraries(${name} PRIVATE shmipc_shared ipc_test Threads::Threads)
  else()
    target_link_libraries(${name} PRIVATE shmipc_static ipc_test Threads::Threads)
  endif()

  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_shmipc_test(ipc_buffer_basic_test         ipc_buffer_basic_test.c)
add_shmipc_test(ipc_buffer_concurrency_test   ipc_buffer_concurrency_test.cpp)
add_shmipc_test(ipc_channel_basic_test        ipc_channel_basic_test.c)
add_shmipc_test(ipc_channel_concurrent_test   ipc_channel_concurrency_test.cpp)
add_shmipc_test(ipc_mmap_test                 ipc_mmap_test.c)
