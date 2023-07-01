# Consuming the Windows MIDI Services SDK

The Windows MIDI Services SDK is built using C++/WinRT. WinRT, a requirement for modern APIs on Windows, enables desktop applications, regardless of language, to be able to use APIs, SDKs, etc. that we create. The older tools, C++/CX, are arguably simpler to implement in, but because they include proprietary extensions to C++, we decided to go with standards-based C++/WinRT instead.

## Prerequisites

To use the SDK (or underlying API) your application language and tools must be able to work with WinRT metadata and libraries.

* Visual Studio 2022 if you are using Visual Studio
* Windows SDK 10.0.20348 (Install with Visual Studio)
* Windows 10 22H2, or preferably, the latest version of Windows 11. Our development machines are all running Windows 11.
* C++ 17 (C++ 20 may work, C++ 14 will not)

Note that there are somewhat hacky ways to get traditional C to work with the COM interfaces, but it is a ton of work for you, and is not a scenario we support. If you find yourself in that situation, I recommend factoring out the MIDI code into its own lib and encapsulating all the C++ calls in there.

## Notes on WinRT

When running inside Visual Studio there's a lot that is forgiven by the tools.

* You can have C++ WinRT to C++ WinRT project references
* Type activation works without a manifest file entry even when not a packaged application

When run outside the tools, you'll need to be prepared to either use a packaged application (UWP or a desktop MSIX with an identity) or for those with new or existing non-packaged desktop apps, add an *appname*.exe.manifest file which lists the classes you need to activate. **We will provide one here on GitHub and keep it updated with the full list of types for all the SDK assemblies.** But, for example, the .manifest file will look something like this:

```xml
<?xml version="1.0" encoding="utf-8"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
    <assemblyIdentity version="1.0.0.0" name="MyArbitraryButUniqueApplicationName.app"/>
    <file name="Windows.Devices.Midi2.dll">		
		<!-- These aren't needed inside Visual Studio -->
		<activatableClass name="Windows.Devices.Midi2.MidiSession" threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
		<activatableClass name="Windows.Devices.Midi2.MidiSessionSettings" threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />

		<activatableClass name="Windows.Devices.Midi2.MidiUmpWithTimestamp" threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
        ...
	</file>
</assembly>
```

| Scenario | VS Project Reference | NuGet Projection DLL Reference | winmd Reference | App manifest entries needed |
| -------- | ------------------| -------------------- | --------------- | ------------------- |
| C++/WinRT app in Visual Studio | Yes | No | Yes | No |
| C#/WinRT app in Visual Studio | No | Yes | No | No |
| Unpackaged Desktop C++ / WinRT App | No | No | Yes | Yes |
| Appx/MSIX Packaged Desktop C++ / WinRT App | No | No | Yes | No |
| Appx/MSIX Packaged Desktop .NET / WinRT App | No | Yes | No | No |

## Consuming from C++

Add the C++/WinRT Nuget package to your C++ project in Visual Studio. This installs the required tools and build process. See the C++/WinRT FAQ link below for using LLVM/Clang. Note that the Windows MIDI Services team does not provide any support for LLVM/Clang, but we will take PRs as required if we need to change something reasonable to ensure you are successful with those tools, within what C++/WinRT can support.

Download the compiled winmd packages from Github (see releases for the latest).

Modify the project file as required (info in the C++/WinRT docs, and you can also look at the sample application code). If you are not using Visual Studio as your toolchain for your project, you may want to pull out the MIDI code into a library in your project which does. It's not strictly required, but it's much easier. (If you do not want to do this, you'll need to manually set up the cppwinrt tools as part of your build process).

> Tip: you can look at the SDK tests for up-to-date project files which target the SDKs in the same solution

[Read through this page](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/consume-apis), specifically the "If the API is implemented in a Windows Runtime component". For the SDK, the calls are NOT in a Windows namespace, although that section can be useful to read.

After that, you reference the types as you would anything else in C++. Only the toolchain is an extra step. What it produces is standard C++. We're considering what we can do here to possibly eliminate even that step in the future, but it's required for now.

* [C++ Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/cpp-winrt/)
* [Introduction to C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/)
* [C++/WinRT on GitHub](https://github.com/microsoft/cppwinrt)
* [C++/WinRT FAQ](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/faq)
* [C++/WinRT Troubleshooting](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/troubleshooting)

## Consuming from C# Desktop App

Your project will currently need to target .NET 7 or above. We are considering support for .NET Framework and lower versions of .NET like .NET 6. However, that is neither confirmed nor promised.

Releases will eventually be in the official Nuget.org package source. For now, you can create a local package source and place the NuGet package in there. Then add it to your package sources in the NuGet Package Manager in Visual Studio.

The package contains the .NET (C#) projection for .NET 7 and above. You will still need to install the C#/WinRT NuGet package in your project because we leverage Windows.Devices.Enumeration and other Windows SDK types.

**Note that other .NET languages (like Visual Basic) may work, but have not been tested.**

* [C# Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/csharp-net/)
* [C#/WinRT on GitHub](https://github.com/microsoft/cswinrt)

## Consuming from C# UWP

Support for this is not yet in place. We are evaluating.

## Consuming from RS

We will provide more information in the future. However, you will follow a similar approach to C++ using RS/WinRT instead of C++/WinRT. Note that teh Rust WinRT tools are newer and are still in active development. Supporting non-Windows SDK winmd files is or will be supported, but is not intuitive at the moment. **There is no existing crate for Windows MIDI Services right now.**

* [Rust Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/rust-winrt/)
* [RS/WinRT on GitHub](https://github.com/microsoft/windows-rs)
