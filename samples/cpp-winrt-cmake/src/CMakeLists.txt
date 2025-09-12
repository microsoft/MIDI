# Run this in a Visual Studio command prompt (just cmd with the environment variables)
# so that VCPKG_ROOT and other variables are set. Or, you can manually set them in
# whatever way you do today. See vcpkg links below for what's required.

cmake_minimum_required (VERSION 3.31)

project(
    MidiCMakeExample
    LANGUAGES CXX
    VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)

include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

# Helpful cmake/vcpkg docs: 
#    https://learn.microsoft.com/vcpkg/users/buildsystems/cmake-integration
#    https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell
#    https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode?tabs=msbuild%2Cbuild-MSBuild
# cppwinrt is configured via vcpkg and so will be downloaded when needed

# you must find and configure cppwinrt before you configure the Windows MIDI Services SDK

find_package(cppwinrt CONFIG REQUIRED)
message("cppwinrt location: ${CPPWINRT_TOOL}")

# get the MIDI SDK (currently from GitHub, but will be on NuGet.org later)
# you can also point this to a local file share if you want to build to a
# specific version that you keep on your build server

find_package(microsoft-windows-devices-midi2 CONFIG REQUIRED)
message("midisdk winmd:    ${MIDI2_WINMD}")

# actual project files
add_executable(MidiCMakeExample "Test.cpp")
#target_include_directories(MidiCMakeExample PRIVATE "${WINRT_GENERATED_HEADERS_FOLDER}")

# for WinRT support
target_link_libraries(MidiCMakeExample PRIVATE "OneCoreUap.lib")
