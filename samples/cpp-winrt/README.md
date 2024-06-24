# Using Windows MIDI Services from C++ / WinRT

It's expected that C++/WinRT is the primary way developers will use this API and SDK. Unlike the older C++/CX ir WRL, C++/WinRT is based on C++ 17 language standards and is implemented as NuGet packages and standard C++ headers.

> From docs:
>C++/WinRT is the recommended alternative to C++/CX. It is a new, standard C++17 language projection for Windows Runtime APIs, available in the latest Windows SDK from version 1803 (10.0.17134.0) onward. C++/WinRT is implemented entirely in header files, and designed to provide you with first-class access to the modern Windows API.
>
> With C++/WinRT, you can both consume and author Windows Runtime APIs using any standards-conformant C++17 compiler. C++/WinRT typically performs better and produces smaller binaries than any other language option for the Windows Runtime. We will continue to support C++/CX and WRL, but highly recommend that new applications use C++/WinRT. For more information, see C++/WinRT.

For C++, Windows MIDI Services is only being tested with C++/WinRT, not CX or WRL, but those will likely still work.

## Samples

| Sample | Description |
| -------| ----------- |
| [App SDK Basics](basics/) | Shows how to open a MidiSession, and connect to a MidiEndpoint to send and receive messages|
| [Enumerate Endpoints](static-enum-endpoints/) | Demonstrates enumerating endpoints, getting group terminal and function blocks, and other properties|
| [Watch Endpoints](watch-endpoints/) | Demonstrates handling device add/remove notifications, as well as device property changes. Most applications will want to use the watcher so they can properly react to device plug/unplug, as well as things like Function Block updates.|

## Background information

* [Get Started with C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/get-started)

