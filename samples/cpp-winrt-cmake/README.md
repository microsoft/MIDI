This sample shows how to use the Windows MIDI Services SDK using cmake and vcpkg. 

This is using our published vcpkg port, which is a convenience wrapper around the NuGet package, targeting C++ developers using cmake.

> This installs only the winmd and the projection headers required for compilation and build systems. It does not install the runtime package or tools, which will be required for use and/or testing. You will want to install and manage those like you would any other runtime dependency.

Requirements
- working cmake install
- vcpkg installed and enabled, with appropriate environment variables set up for vcpkg 
    - For convenience, you can use a Visual Studio command prompt, where all the environment variables are already set up
- cmake run from a command prompt that has the right environment variables set for msbuild
- internet access to download the cppwinrt and microsoft-windows-devices-midi2 vcpkg ports

We recommend C++ 20, but require a minimum of C++ 17. The `cppwinrt.exe` tool produces headers which light up additional optimizations when using C++ 20.

## Setup

In your project src folder for the component which will bind to the MIDI SDK

Create a new vcpkg json file

`vcpkg new --application`

Add cppwinrt to the vcpkg.json file. This includes the latest header generator `cppwinrt.exe`. There's also a version installed in the Windows SDK, but it's typically much older than the one available via vcpkg / nuget. **The MIDI2 port expects to find cppwinrt installed via vcpkg.**

`vcpkg add port cppwinrt`

`vcpkg add port microsoft-windows-devices-midi2`

The resulting vcpkg.json file will look like this

```json
{
  "dependencies": [
    "cppwinrt",
    "microsoft-windows-devices-midi2"
  ]
}
```

## Using Specific Versions

If you need to lock to a specific version of either port, see the [vcpkg documentation](https://learn.microsoft.com/vcpkg/commands/add) and the [vcpkg manifest format here](https://learn.microsoft.com/vcpkg/concepts/manifest-mode).

