set(SOURCES
  ipc_buffer.c
  ipc_mmap.c
)

set(OUTPUT_NAME shmipc)

add_library(shm_ipc_static STATIC ${SOURCES})
set_target_properties(shm_ipc_static PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

add_library(shm_ipc_shared SHARED ${SOURCES})
set_target_properties(shm_ipc_shared PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

target_include_directories(shm_ipc_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(shm_ipc_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

