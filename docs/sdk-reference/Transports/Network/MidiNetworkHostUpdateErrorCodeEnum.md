---
layout: sdk_reference_page
title: MidiNetworkHostUpdateErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when starting or stopping a Network MIDI 2.0 host
---

Returned in `MidiNetworkHostUpdateResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `HostNotFound` | `0x00001065` | No host with this id is set up |
| `UnableToStartHost` | `0x00000023` | The host couldn't be started |
| `UnableToStopHost` | `0x00000024` | The host couldn't be stopped |
| `NetworkAdapterNotAvailable` | `0x00000025` | The host's network adapter is missing, and the host isn't allowed to use the others. It starts by itself when the adapter is back |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `HostId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `HostId` wasn't a valid GUID |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
