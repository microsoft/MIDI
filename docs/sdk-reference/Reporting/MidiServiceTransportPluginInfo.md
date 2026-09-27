---
layout: sdk_reference_page
title: MidiServiceTransportPluginInfo
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Information about a single transport plugin loaded in the MIDI Service
---

The MIDI Console and MIDI Settings mostly use this to show which transports are installed. MIDI Settings also uses it to decide which settings to show for each transport.

If you write a new transport, these values come from your plugin, through its `IMidiServiceTransportPluginMetadataProvider` interface.

## Properties

| Property | Description |
| --- | --- |
| `TransportId` | A GUID for the transport |
| `Name` | The transport's name |
| `TransportCode` | A short code for the transport, such as "KSA" |
| `Description` | The transport's description |
| `ImageFileName` | An image for the transport. It isn't used yet, but future versions of MIDI Settings will use it |
| `Author` | Who made the transport |
| `Version` | The version of the transport. It's for display only |
| `IsRuntimeCreatableByApps` | True if applications can create endpoints for this transport |
| `IsRuntimeCreatableBySettings` | True if MIDI Settings can create endpoints for this transport, and save them to the configuration |
| `IsSystemManaged` | True if Windows manages this transport, like the diagnostics transport |
| `CanConfigure` | True if the transport can be set up in MIDI Settings |

## Samples

Make decisions in your code based on `TransportCode`, not on `Name` or `Description`. Those are text for people to read, and they can change.

* [C++/WinRT identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/identify-endpoint-type)
* [C# identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/identify-endpoint-type)
