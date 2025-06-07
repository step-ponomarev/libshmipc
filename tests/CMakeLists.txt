set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ipc_test STATIC test_runner.c)


add_executable(test_ipc_buffer test_ipc_buffer.c)
target_link_libraries(test_ipc_buffer ipc ipc_test)
add_test(NAME test_ipc_buffer COMMAND test_ipc_buffer)
