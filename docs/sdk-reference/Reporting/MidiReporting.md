---
layout: sdk_reference_page
title: MidiReporting
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Provides information about the service configuration
---

`MidiReporting` tells you about the MIDI service: which transports are installed, which sessions are open, and what those sessions are using.

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `GetInstalledTransportPlugins()` | Returns a `MidiServiceTransportPluginInfo` for every transport installed in the service |
| `GetActiveSessions()` | Returns a `MidiServiceSessionInfo` for every Windows MIDI Services session open on this PC |
| `FindAllSessionsWithMatchingOpenUmpEndpoint(endpointDeviceId, includeRelatedMidi1Ports)` | Returns a `MidiServiceSessionInfo` for every session that has this UMP endpoint open right now. When `includeRelatedMidi1Ports` is true, it also includes sessions that have only the endpoint's MIDI 1.0 ports open |
| `FindAllSessionsWithMatchingOpenUmpEndpointOrMidi1Ports(endpointsAndPorts)` | Returns a `MidiServiceSessionInfo` for every session that has any of these endpoint or MIDI 1.0 port ids open |

## Remarks

The two `FindAllSessions...` methods answer the question "who is using this device?" You need that answer to warn people that changing or removing an endpoint will affect an application that's running.

## Samples

Match the results of `GetInstalledTransportPlugins` against an endpoint's `TransportId` to show people what kind of device they're looking at.

* [C++/WinRT identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/identify-endpoint-type)
* [C# identify-endpoint-type](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/identify-endpoint-type)
