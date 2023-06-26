# Consuming the Windows MIDI Services SDK

To use the SDK (or underlying API) your application language and tools must be able to work with WinRT metadata and libraries.

## Consuming from C++

Add the C++/WinRT Nuget package to your C++ project.

Download the compiled winmd packages from Github. (We may offer another way to handle this in the future, but Nuget packages can't modify C++ projects in the same way they can C# and others)

Modify the project file as required. If you are not using Visual Studio as your toolchain for your project, you may want to pull out the MIDI code into a library in your project which does. It's not strictly required, but it's much easier. (If you do not want to do this, you'll need to manually set up the cppwinrt tools as part of your build process).

[Read through this page](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/consume-apis), specifically the "If the API is implemented in a Windows Runtime component". For the SDK, the calls are NOT in a Windows namespace, although that section can be useful to read.

After that, you reference the types as you would anything else in C++. Only the toolchain is an extra step. What it produces is standard C++. We're considering what we can do here to possibly eliminate even that step in the future, but it's required for now.

* [C++ Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/cpp-winrt/)
* [Introduction to C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/)
* [C++/WinRT on GitHub](https://github.com/microsoft/cppwinrt)


## Consuming from C# Desktop App

Your project will currently need to target .NET 7 or above. We are considering support for .NET Framework and lower versions of .NET like .NET 6. However, that is neither confirmed nor promised.

Releases will eventually be in the official Nuget.org package source. For now, you can create a local package source and place the NuGet package in there. Then add it to your package sources in the NuGet Package Manager in Visual Studio.

The package contains the .NET (C#) projection for .NET 7 and above.

**Note that other .NET languages (like Visual Basic) may work, but have not been tested.**

* [C# Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/csharp-net/)
* [C#/WinRT on GitHub](https://github.com/microsoft/cswinrt)


## Consuming from C# UWP

Support for this is not yet in place. We are evaluating.

## Consuming from RS

We will provide more information in the future. However, you will follow a similar approach to C++ using RS/WinRT instead of C++/WinRT. Note that teh Rust WinRT tools are newer and are still in active development. Supporting non-Windows SDK winmd files is or will be supported, but is not intuitive at the moment. **There is no existing crate for Windows MIDI Services right now.**

* [Rust Windows MIDI Services Example Code](https://github.com/microsoft/midi/get-started/midi-developers/app-developers/samples/rust-winrt/)
* [RS/WinRT on GitHub](https://github.com/microsoft/windows-rs)

