add_library(ipc STATIC
  ipc_buffer.c  
  ipc_mmap.c
)

target_include_directories(ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

