add_library(ipc_test STATIC test_runner.c)

add_executable(ipc_buffer_basic_test ipc_buffer_basic_test.c)
target_link_libraries(ipc_buffer_basic_test ipc ipc_test)
add_test(NAME test_ipc_buffer COMMAND ipc_buffer_basic_test)
