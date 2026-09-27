---
layout: sdk_reference_page
title: MidiNetworkClientDisconnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when disconnecting a Network MIDI 2.0 client
---

Returned in `MidiNetworkClientDisconnectResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `ClientNotFound` | `0x00001066` | No client with this id is connected |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `ClientId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `ClientId` wasn't a valid GUID |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
