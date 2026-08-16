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
| `UnrecognizedCommand` | `0x00000001` | The command sent to the service was not recognized |
| `ClientNotFound` | `0x00001066` | No client with this identifier is connected |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `ClientId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `ClientId` was not a valid GUID |
| `InvalidArgument` | `0x11000055` | An invalid argument was supplied by the caller |
| `ClientApiException` | `0x11002011` | An exception occurred in the client API |
