---
layout: kb
title: Use the Windows MIDI Services SDK with cmake and vcpkg
audience: developers
description: How to integrate Microsoft.Windows.Devices.Midi2 into cmake and other C++ pipelines
---

The primary distribution method for the Windows MIDI Services App SDK build-time dependencies is NuGet. The NuGet package contains the C# projection, as well as the required WinMD files, the initialization .hpp file, and the version information header.

However, using NuGet packages in C++ outside of Visual Studio can be very challenging.

To more easily integrate into cmake, and other vcpkg-aware tools, there is now a vcpkg wrapper around the NuGet package. The wrapper downloads cppwinrt and the SDK package, and generates the headers right there, in a vcpkg and cmake-friendly way.

## vcpkg

The vcpackage is `microsoft-windows-devices-midi2` and may be found [here](https://vcpkg.io/en/package/microsoft-windows-devices-midi2). 

## cmake sample

The cmake example, and the accompanying readme, contain the instructions needed to integrate the vcpkg into your build pipeline. This has been tested with msbuild and msvc++. If you use other C++ toolchains and run into issues, please file a bug with clear setup and repro instructions on [GitHub](https://aka.ms/midirepo).

[All samples are located here](https://aka.ms/midisamples)