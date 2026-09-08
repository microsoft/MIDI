# CSharp / .NET Samples

Using C# / WinRT and the latest versions of .NET to use the Windows MIDI Services API.

## Samples

| Sample | Description |
| -------| ----------- |
| [Basics](basics/) | Shows how to open a MidiSession, and connect to a MidiEndpoint to send and receive messages |
| [Endpoint Listeners](endpoint-listeners/) | Demonstrates filtering incoming messages down to specific groups, which is how you emulate a MIDI 1.0 port |
| [Get VID and PID](get-vid-pid/) | Shows where to find the USB vendor and product ids for an endpoint. Replaces the WinMM `DRV_QUERYDEVICEINTERFACE` approach |
| [Identify Endpoint Type](identify-endpoint-type/) | Correlates each endpoint with the transport which created it, so you can tell USB from network from virtual |
| [Loopback Basic Endpoints](loopback-basic-endpoints/) | Creates a MIDI 1.0-style loopback endpoint at runtime, visible to older MIDI APIs |
| [Loopback Endpoints](loopback-endpoints/) | Creates a pair of MIDI 2.0 bidirectional loopback endpoints at runtime |
| [Message Light WPF](message-light-wpf/) | A small WPF application which flashes on incoming messages |
| [Scheduled Send Messages](scheduled-send-messages/) | Schedules messages for delivery at a future timestamp. There is no WinMM equivalent for this |
| [Send Speed](send-speed/) | Measures message sending throughput |
| [Static Enum Endpoints](static-enum-endpoints/) | Gets a one-time list of active endpoints, with function blocks and group terminal blocks |
| [SysEx File Receiver](sysex-file-receiver/) | Receives a System Exclusive message and writes it to a `.syx` file |
| [SysEx File Sender](sysex-file-sender/) | Sends the contents of a `.syx` file to a device. Replaces WinMM `midiOutLongMsg` and `MIDIHDR` buffer management |
| [SysEx Send Bytes](sysex-send-bytes/) | Converts MIDI 1.0 bytes you already have in memory into UMP words, and sends them |
| [Virtual Device App (WinUI)](virtual-device-app-winui/) | A more real-world WinUI application which creates and manages a virtual MIDI device |
| [Watch Endpoints](watch-endpoints/) | Handles device add, remove and property-change notifications. Most applications will want the watcher rather than a static list |
| [Watch MIDI 1 Ports](watch-midi1-ports/) | Watches the MIDI 1.0 port list that older APIs see, with proper notifications instead of polling |

The C++/WinRT samples cover the same ground, and additionally demonstrate the COM Extensions, which
are only available to C++ and other COM-aware languages.

## Building the Samples

We recommend you load the entire solution in Visual Studio, as that is how the NuGet package
configuration is set up. Change the sample you run by setting that project as startup.

Before running, you will need the current Windows MIDI Services NuGet package, in a location on your
PC configured as a NuGet package source. The package is available from the
[releases page](https://github.com/microsoft/MIDI/releases).

## Background information

* [C#/WinRT](https://learn.microsoft.com/windows/apps/develop/platform/csharp-winrt/)
