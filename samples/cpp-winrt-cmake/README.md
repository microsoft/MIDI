This sample shows how to use the Windows MIDI Services SDK using cmake and vcpkg. 

This is using our published vcpkg port, which is a convenience wrapper around the NuGet package, targeting C++ developers using cmake.

Requirements
- working cmake install
- vcpkg installed and enabled, with appropriate environment variables set up for vcpkg
- cmake run from a command prompt that has the right environment variables set for msbuild
- internet access to download the cppwinrt and microsoft-windows-devices-midi2 vcpkg ports

In your project src folder for the component which will bind to the MIDI SDK

Create a new vcpkg json file

`vcpkg new --application`

Add cppwinrt to the vcpkg.json file

`vcpkg add port cppwinrt`

`vcpkg add port microsoft-windows-devices-midi2`

See the cmake files for further steps.
