#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "shmipc::shmipc_static" for configuration ""
set_property(TARGET shmipc::shmipc_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(shmipc::shmipc_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libshmipc_static.a"
  )

list(APPEND _cmake_import_check_targets shmipc::shmipc_static )
list(APPEND _cmake_import_check_files_for_shmipc::shmipc_static "${_IMPORT_PREFIX}/lib/libshmipc_static.a" )

# Import target "shmipc::shmipc_shared" for configuration ""
set_property(TARGET shmipc::shmipc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(shmipc::shmipc_shared PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libshmipc_shared.0.0.0.dylib"
  IMPORTED_SONAME_NOCONFIG "@rpath/libshmipc_shared.0.dylib"
  )

list(APPEND _cmake_import_check_targets shmipc::shmipc_shared )
list(APPEND _cmake_import_check_files_for_shmipc::shmipc_shared "${_IMPORT_PREFIX}/lib/libshmipc_shared.0.0.0.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
