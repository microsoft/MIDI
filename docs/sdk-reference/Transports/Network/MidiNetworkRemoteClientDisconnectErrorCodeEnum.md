---
layout: sdk_reference_page
title: MidiNetworkRemoteClientDisconnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes for disconnecting a remote client from a host on this PC
---

Used by `MidiNetworkRemoteClientDisconnectResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service did not recognize the command |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | Host identifier is missing or invalid |
| `MalformedEntryIdentifier` | `0x00000032` | Host identifier format is malformed |
| `InvalidOrMissingRemoteClientIdentity` | `0x00000073` | Required remote client identity fields are missing |
| `HostNotFound` | `0x00001065` | The target host entry was not found |
| `RemoteClientNotFound` | `0x00001067` | The target remote client is not currently connected to that host |
| `InvalidArgument` | `0x11000055` | One or more arguments were invalid |
| `ClientApiException` | `0x11002011` | A client-side API exception occurred while processing the request |
