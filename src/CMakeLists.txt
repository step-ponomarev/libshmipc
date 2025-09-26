set(SHMIPC_SOURCES
  ipc_buffer.c
  ipc_channel.c
  ipc_utils.c
  ipc_mmap.c
)

set(SHMIPC_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

function(configure_library_common tgt)
  target_include_directories(${tgt}
    PUBLIC
      $<BUILD_INTERFACE:${SHMIPC_PUBLIC_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  if (BUILD_SHARED_LIBS OR SHMIPC_BUILD_SHARED)
    set_target_properties(${tgt} PROPERTIES
      C_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES)
  endif()
 
  set_target_properties(${tgt} PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

  target_enable_warnings(${tgt})

  set_property(TARGET ${tgt} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

if (SHMIPC_BUILD_STATIC)
  add_library(shmipc_static STATIC ${SHMIPC_SOURCES})
  add_library(shmipc::static ALIAS shmipc_static)
  configure_library_common(shmipc_static)
endif()

if (SHMIPC_BUILD_SHARED)
  add_library(shmipc_shared SHARED ${SHMIPC_SOURCES})
  add_library(shmipc::shared ALIAS shmipc_shared)
  configure_library_common(shmipc_shared)
endif()

set(_install_targets)
if (TARGET shmipc_static)
  list(APPEND _install_targets shmipc_static)
endif()
if (TARGET shmipc_shared)
  list(APPEND _install_targets shmipc_shared)
endif()

install(TARGETS ${_install_targets}
  EXPORT shmipcTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${SHMIPC_PUBLIC_INCLUDE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
