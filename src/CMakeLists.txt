set(SOURCES ipc_buffer.c ipc_channel.c ipc_utils.c)

set(OUTPUT_NAME shmipc)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DEST ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


add_library(shm_ipc_static STATIC ${SOURCES})
set_target_properties(
  shm_ipc_static PROPERTIES OUTPUT_NAME ${OUTPUT_NAME}
)

add_library(shm_ipc_shared SHARED ${SOURCES})
set_target_properties(
  shm_ipc_shared PROPERTIES OUTPUT_NAME ${OUTPUT_NAME} 
)

target_include_directories(shm_ipc_static PUBLIC ${INCLUDE_DIR})
target_include_directories(shm_ipc_shared PUBLIC ${INCLUDE_DIR})

# Установка: заголовки и библиотеки
install(TARGETS shm_ipc_static shm_ipc_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

# Обязательно слэш на конце!
install(DIRECTORY ${INCLUDE_DIR}/ DESTINATION include)

