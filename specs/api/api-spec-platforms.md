# API Specifications: Supported developer platforms

Supported developer platforms.

The primary focus of this API is to support desktop apps on Windows. 

# C++ and other similar languages

Standard C++ using the C++/WinRT library

# UWP / WinRT

Directly consume the WinRT projections

# .NET

Support .NET 7 at a minimum. Eventually move code to and stabilize on .NET 8 in a future revision, assuming it's the long-term servicing version at the time. Down-level support for consuming apps will be .NET 7+ or possibly .NET 6+, depending upon compatibility requirements for some of the memory sharing mechanisms.

No planned support for .NET Framework. It may work, but will not be supported.

# .NET MAUI

Investigate. Should be available through WinRT support, if not native .NET

# Browser

Use of the new API will require a Web MIDI 2.0 specification, first.