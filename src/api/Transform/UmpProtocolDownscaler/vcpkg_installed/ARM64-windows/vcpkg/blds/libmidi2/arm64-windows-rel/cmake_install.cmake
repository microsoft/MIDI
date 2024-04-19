# Install script for directory: G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/pkgs/libmidi2_arm64-windows")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/arm64-windows-rel/libmidi2.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/libmidi2/libmidi2-config.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/libmidi2/libmidi2-config.cmake"
         "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/arm64-windows-rel/CMakeFiles/Export/0d7ad336f582a14afd488c3a7811f6bb/libmidi2-config.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/libmidi2/libmidi2-config-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/libmidi2/libmidi2-config.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/libmidi2" TYPE FILE FILES "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/arm64-windows-rel/CMakeFiles/Export/0d7ad336f582a14afd488c3a7811f6bb/libmidi2-config.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/libmidi2" TYPE FILE FILES "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/arm64-windows-rel/CMakeFiles/Export/0d7ad336f582a14afd488c3a7811f6bb/libmidi2-config-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libmidi2" TYPE FILE FILES
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/bytestreamToUMP.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/mcoded7.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/midiCIMessageCreate.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/midiCIProcessor.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/umpMessageCreate.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/umpProcessor.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/umpToBytestream.h"
    "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/src/v0.7-9d11c5604d.clean/include/utils.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "G:/GitHub/microsoft/midi/src/api/Transform/UmpProtocolDownscaler/vcpkg_installed/ARM64-windows/vcpkg/blds/libmidi2/arm64-windows-rel/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
