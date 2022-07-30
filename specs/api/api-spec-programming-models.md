# API Specifications: Supported developer platforms

Supported developer platforms.

The primary focus of this API is to support desktop apps on Windows.

Currently, the API is authored in .NET 7 (not to be confused with the older .NET Framework). If it continues with that path, .NET support is free. .NET has advantages and disadvantages, but two of the biggest advantages are:

* Tools, languages, and libraries make it easier to quickly author safe code for the platform
* The .NET developer community is full of developers who can contribute to these projects.
* .NET Runs on Windows, macOS, and Linux making it easier to port code should anyone want to reuse the library on those platforms.

That said, if we decide any pieces need to be C++ for whatever reason, we will port that part.

## Primary interfaces (COM, .NET)

As such, it will ship with a minimum of:

COM or WinRT interface library, depending on what works best. WinRT is nice because it can provide projections which work across most any language. However, we will be relying on C#/WinRT and possibly C++/WinRT to accomplish this. The former is in preview and the latter requires more code in the SDK.

Note also that WinRT can be cumbersome for other apps to consume, despite it being a modern API approach.

* [Supporting WinRT and COM using C++/WinRT](https://docs.microsoft.com/windows/uwp/cpp-and-winrt-apis/author-coclasses)
* [Exposing .NET Components to COM](https://docs.microsoft.com/dotnet/core/native-interop/expose-components-to-com)

## .NET (desktop app)

For .NET, we will support .NET 7 at a minimum. Eventually move code to and stabilize on .NET 8 in a future revision, assuming it's the long-term servicing version at the time. Down-level support for consuming apps will be .NET 7+ or possibly .NET 6+, depending upon compatibility requirements for some of the memory sharing mechanisms.

No planned support for .NET Framework. It may work, but will not be supported.

## UWP

If we can easily support UWP, we will, but it is not a priority target. Instead, we recommend developers use desktop .NET with the WinUI library for new apps.

It's possible that a future version of Windows will contain a shim from Windows.Devices.Midi to the new API, with limited functionality.

## .NET MAUI

Investigate. Should be available through WinRT support, if not native .NET

## Browser

Use of the new API will require a Web MIDI 2.0 specification, first.
