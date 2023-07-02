# Building the MIDI SDK for yourself

The Windows MIDI Services SDK is built using C++/WinRT. WinRT, a requirement for modern APIs on Windows, enables desktop applications, regardless of language, to be able to use APIs, SDKs, etc. that we create. The older tools, C++/CX, are arguably simpler to implement in, but because they include proprietary extensions to C++, we decided to go with standards-based C++/WinRT instead.

Normally, you'll just one of the SDK packages. However, if you wish to build it for yourself, this document explains how.

## Prerequisites

To build the SDK you need to use Visual Studio 2022 and C++/WinRT

* Visual Studio 2022 if you are using Visual Studio
* Windows SDK 10.0.20348 (Install with Visual Studio)
* Windows 10 22H2, or preferably, the latest version of Windows 11. Our development machines are all running Windows 11.
* C++ 17 (C++ 20 may work, C++ 14 will not)
* Boost 1.82 or higher installed in %BOOST_ROOT% (and that environment variable set, of course)
* C++/WinRT will be installed via NuGet

## Folder Structure

Here's the structure on my developer PC.

github\microsoft\midi is the root of the repo. Inside that, the src, get-started, etc. folders from GitHub.

github\microsoft\midi\publish is a folder that doesn't appear in the repo, but is referenced by the NuGet build. This is where the NuGet packages end up

github\microsoft\midi\src\app-dev-sdk\_build is the build target location for SDK projects. This is where the NuGet packaging steps expect to find the binaries for packaging.

## Building

The midi-sdk solution is the main solution for building, well, the MIDI SDK. It includes the correct runsettings for tests, and the Directory.Build.props files for the build layout.

For the native projects, note that 32 Bit Arm and x86 (32 bit) architectures are not supported by this project. You may build for x64, Arm64, (and in the future, Arm64EC). We are intentionally not building for 32 bit architectures here.

NuGet packages are set to pick up only Release output, not Debug. To get it all going, do a batch build of the supported architectures.

## Tests

To run the Catch2 unit tests, you'll need the Catch2 Test Adapter VSIX installed. It's called "Test Adapter for Catch2" and is available in the Visual Studio Marketplace.

Open the Test Explorer and then build the full solution if tests do not appear in it. You can then execute tests.