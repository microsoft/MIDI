# Using Windows MIDI Services from C++ / WinRT

It's expected that C++/WinRT is the primary way developers will use this API and SDK. Unlike the older C++/CX or WRL, C++/WinRT is based on C++ 17 language standards and is implemented as NuGet packages and standard C++ headers.

> From docs:
>C++/WinRT is the recommended alternative to C++/CX. It is a new, standard C++17 language projection for Windows Runtime APIs, available in the latest Windows SDK from version 1803 (10.0.17134.0) onward. C++/WinRT is implemented entirely in header files, and designed to provide you with first-class access to the modern Windows API.
>
> With C++/WinRT, you can both consume and author Windows Runtime APIs using any standards-conformant C++17 compiler. C++/WinRT typically performs better and produces smaller binaries than any other language option for the Windows Runtime. We will continue to support C++/CX and WRL, but highly recommend that new applications use C++/WinRT. For more information, see C++/WinRT.

For C++, Windows MIDI Services is only being tested with C++/WinRT, not CX or WRL, but those will likely still work.

## Samples

| Sample | Description |
| -------| ----------- |
| [Basics](basics/) | Shows how to open a MidiSession, and connect to a MidiEndpoint to send and receive messages |
| [COM Extensions](com-extensions/) | Shows how to use the COM extensions for super fast allocation-free MIDI Message send/receive |
| [Endpoint Listeners](endpoint-listeners/) | Demonstrates filtering incoming messages by group, channel, or message type |
| [Get VID and PID](get-vid-pid/) | Shows where to find the USB vendor and product ids for an endpoint. Replaces the WinMM `DRV_QUERYDEVICEINTERFACE` approach |
| [Identify Endpoint Type](identify-endpoint-type/) | Correlates each endpoint with the transport which created it, so you can tell USB from network from virtual |
| [Loopback Basic Endpoints](loopback-basic-endpoints/) | Demonstrates how to create MIDI 1.0-style loopback endpoints at runtime |
| [Loopback Basic Endpoints from WinMM](loopback-basic-endpoints-winmm/) | Creates loopback endpoints and then uses them from the older WinMM API |
| [Loopback Endpoints](loopback-endpoints/) | Demonstrates how to create MIDI 2.0 bidirectional loopback endpoints at runtime |
| [Scheduled Messages using COM Extensions](scheduled-messages-com-extensions/) | Schedules messages for future delivery using the allocation-free COM extensions |
| [Scheduled Send Messages](scheduled-send-messages/) | Schedules messages for delivery at a future timestamp. There is no WinMM equivalent for this |
| [Send Speed](send-speed/) | Measures message sending throughput |
| [Simple app-to-app MIDI](simple-app-to-app-midi/) | Demonstrates how to create an application endpoint, and update properties like function blocks. The C# sample is more of a real-world use-case |
| [Static Enum Endpoints](static-enum-endpoints/) | Demonstrates how to get a one-time static list of active endpoints, with group terminal blocks, function blocks, and other properties |
| [SysEx File Receiver](sysex-file-receiver/) | Receives a System Exclusive message and writes it to a `.syx` file |
| [SysEx File Sender](sysex-file-sender/) | Sends the contents of a `.syx` file to a device. Replaces WinMM `midiOutLongMsg` and `MIDIHDR` buffer management |
| [SysEx Send Bytes](sysex-send-bytes/) | Converts MIDI 1.0 bytes you already have in memory into UMP words, and sends them |
| [Watch Endpoints](watch-endpoints/) | Demonstrates handling device add/remove notifications, as well as device property changes. Most applications will want to use the watcher so they can properly react to device plug/unplug, as well as things like Function Block updates |
| [Watch MIDI 1 Ports](watch-midi1-ports/) | Watch ports that are available to older MIDI 1 APIs |

We recommend you load the entire solution in Visual Studio, as that is how the NuGet package configuration is set up. Change the sample you run by setting that project as startup.

Before running, you will need the current Windows MIDI Services NuGet package, in a location on your PC configured as a NuGet package source. The package is available from the [releases page](https://github.com/microsoft/MIDI/releases).

## Background information

* [Get Started with C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/get-started)

