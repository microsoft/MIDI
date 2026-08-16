---
layout: sdk_reference_page
title: MidiNetworkClientConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when connecting to a remote Network MIDI 2.0 host
---

Returned in `MidiNetworkClientConnectResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The command sent to the service was not recognized |
| `InvalidJson` | `0x00000011` | The configuration sent to the service was not valid JSON |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `ClientId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `ClientId` was not a valid GUID |
| `InvalidOrMissingRemoteAddress` | `0x00000061` | The remote address was missing for a direct connection |
| `InvalidOrMissingRemotePort` | `0x00000062` | The remote port was missing for a direct connection |
| `RemotePortOutOfRange` | `0x00000063` | The remote port was not a valid port number |
| `InvalidOrMissingMatchCriteria` | `0x00000064` | Neither a device id nor a direct address was supplied |
| `InvalidNetworkProtocol` | `0x00000044` | The network protocol specified is not supported. Only UDP is |
| `NoReplyToInvitation` | `0x00000071` | The remote host never answered the invitation |
| `InvitationNotApproved` | `0x00000072` | The remote host asked us to wait, and the request was never approved |
| `InvalidArgument` | `0x11000055` | An invalid argument was supplied by the caller |
| `ClientApiException` | `0x11002011` | An exception occurred in the client API |
