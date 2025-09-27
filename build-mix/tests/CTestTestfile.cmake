# CMake generated Testfile for 
# Source directory: /Users/stepan/libshmipc/tests
# Build directory: /Users/stepan/libshmipc/build-mix/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ipc_buffer_basic_test "/Users/stepan/libshmipc/build-mix/tests/ipc_buffer_basic_test")
set_tests_properties(ipc_buffer_basic_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/stepan/libshmipc/tests/CMakeLists.txt;39;add_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;42;add_shmipc_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;0;")
add_test(ipc_buffer_concurrency_test "/Users/stepan/libshmipc/build-mix/tests/ipc_buffer_concurrency_test")
set_tests_properties(ipc_buffer_concurrency_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/stepan/libshmipc/tests/CMakeLists.txt;39;add_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;43;add_shmipc_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;0;")
add_test(ipc_channel_basic_test "/Users/stepan/libshmipc/build-mix/tests/ipc_channel_basic_test")
set_tests_properties(ipc_channel_basic_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/stepan/libshmipc/tests/CMakeLists.txt;39;add_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;44;add_shmipc_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;0;")
add_test(ipc_channel_concurrent_test "/Users/stepan/libshmipc/build-mix/tests/ipc_channel_concurrent_test")
set_tests_properties(ipc_channel_concurrent_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/stepan/libshmipc/tests/CMakeLists.txt;39;add_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;45;add_shmipc_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;0;")
add_test(ipc_mmap_test "/Users/stepan/libshmipc/build-mix/tests/ipc_mmap_test")
set_tests_properties(ipc_mmap_test PROPERTIES  _BACKTRACE_TRIPLES "/Users/stepan/libshmipc/tests/CMakeLists.txt;39;add_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;46;add_shmipc_test;/Users/stepan/libshmipc/tests/CMakeLists.txt;0;")
