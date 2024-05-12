#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libmidi2::libmidi2" for configuration "Release"
set_property(TARGET libmidi2::libmidi2 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(libmidi2::libmidi2 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmidi2.lib"
  )

list(APPEND _cmake_import_check_targets libmidi2::libmidi2 )
list(APPEND _cmake_import_check_files_for_libmidi2::libmidi2 "${_IMPORT_PREFIX}/lib/libmidi2.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
