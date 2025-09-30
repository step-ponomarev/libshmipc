cmake_minimum_required(VERSION 3.16)

# ---------- Project ----------
project(
  shmipc
  VERSION 0.0.1
  LANGUAGES C CXX
)

# ---------- Options ----------
option(SHMIPC_BUILD_STATIC     "Build static library"           ON)
option(SHMIPC_BUILD_SHARED     "Build shared library"           ON)
option(SHMIPC_BUILD_TESTS      "Build tests"                    ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- Policies / Nice defaults ----------
function(target_enable_warnings tgt)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(${tgt} PRIVATE -Werror)
endfunction()

# --- pocix fix ----
#TODO remove it
function(target_fix_posix tgt)
  target_compile_definitions(${tgt} PRIVATE
  _POSIX_C_SOURCE=200809L
  _XOPEN_SOURCE=700
  )
endfunction()

# ---------- Install dirs ----------
include(GNUInstallDirs)

# ---------- Testing toggle (CTest) ----------
include(CTest)
if (NOT SHMIPC_BUILD_TESTS)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
endif()

# ---------- Subdirs ----------
add_subdirectory(src)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

# ---------- Package config (export targets for find_package) ----------
include(CMakePackageConfigHelpers)

set(SHMIPC_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/shmipc")

install(EXPORT shmipcTargets
  NAMESPACE shmipc::
  FILE ShmipcTargets.cmake
  DESTINATION ${SHMIPC_INSTALL_CONFIGDIR})

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/ShmipcConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ShmipcConfig.cmake"
  INSTALL_DESTINATION ${SHMIPC_INSTALL_CONFIGDIR}
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ShmipcConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/ShmipcConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ShmipcConfigVersion.cmake"
  DESTINATION ${SHMIPC_INSTALL_CONFIGDIR})

# Метаданные пакета — задаём ДО include(CPack)
set(CPACK_PACKAGE_NAME              "libshmipc")
set(CPACK_PACKAGE_VENDOR            "Stepan Ponomarev")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Shared-memory IPC library")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/step-ponomarev/libshmipc")
set(CPACK_PACKAGE_CONTACT           "step.ponomarev@gmail.com")

#TODO License
#set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_SOURCE_DIR}/LICENSE") 
set(CPACK_GENERATOR                 "TGZ")
set(CPACK_PACKAGE_CHECKSUM        "SHA256")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)

