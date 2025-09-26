cmake_minimum_required(VERSION 3.16)

# ---------- Project ----------
project(shmipc
  VERSION 0.3.0
  LANGUAGES C CXX)

# ---------- Options ----------
option(SHMIPC_BUILD_STATIC     "Build static library"           ON)
option(SHMIPC_BUILD_SHARED     "Build shared library"           ON)
option(SHMIPC_BUILD_TESTS      "Build tests"                    ON)
option(SHMIPC_ENABLE_WERROR    "Treat warnings as errors"       ON)

# Глобально: compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Стандарт языка по умолчанию (можно и на таргеты перенести)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Для C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- Policies / Nice defaults ----------
# Хорошие дефолты предупреждений под разные компиляторы
function(target_enable_warnings tgt)
  if (MSVC)
    target_compile_options(${tgt} PRIVATE /W4)
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if (SHMIPC_ENABLE_WERROR)
    if (MSVC)
      target_compile_options(${tgt} PRIVATE /WX)
    else()
      target_compile_options(${tgt} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# ---------- Install dirs ----------
include(GNUInstallDirs)

# ---------- Testing toggle (CTest) ----------
include(CTest) # создаёт опцию BUILD_TESTING
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

# Экспортируем все таргеты из src
install(EXPORT shmipcTargets
  NAMESPACE shmipc::
  FILE ShmipcTargets.cmake
  DESTINATION ${SHMIPC_INSTALL_CONFIGDIR})

# Генерим Config.cmake и ConfigVersion.cmake
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

# ---------- CPack (пакеты) ----------
include(CPack)
set(CPACK_PACKAGE_NAME "shmipc")
set(CPACK_PACKAGE_VENDOR "YourOrg")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ") # при желании добавь DEB/RPM/DragNDrop и т.д.
