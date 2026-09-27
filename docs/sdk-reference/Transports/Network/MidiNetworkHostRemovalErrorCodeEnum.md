---
layout: sdk_reference_page
title: MidiNetworkHostRemovalErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when removing a Network MIDI 2.0 host
---

Returned in `MidiNetworkHostRemovalResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `HostNotFound` | `0x00001065` | No host with this id is set up |
| `HostRemovalFailed` | `0x00000022` | The host couldn't be removed |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `HostId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `HostId` wasn't a valid GUID |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
