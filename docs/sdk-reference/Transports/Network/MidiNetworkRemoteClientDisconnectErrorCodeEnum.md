---
layout: sdk_reference_page
title: MidiNetworkRemoteClientDisconnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes for disconnecting a remote client from a host on this PC
---

Used by `MidiNetworkRemoteClientDisconnectResponse.ErrorCode`.

## Values

| Value | Description |
| ----- | ----------- |
| `NoErrorInformationAvailable` | No additional error information is available |
| `UnrecognizedCommand` | The service did not recognize the command |
| `HostNotFound` | The target host entry was not found |
| `RemoteClientNotFound` | The target remote client is not currently connected to that host |
| `InvalidOrMissingEntryIdentifier` | Host identifier is missing or invalid |
| `MalformedEntryIdentifier` | Host identifier format is malformed |
| `InvalidOrMissingRemoteClientIdentity` | Required remote client identity fields are missing |
| `InvalidArgument` | One or more arguments were invalid |
| `ClientApiException` | A client-side API exception occurred while processing the request |
