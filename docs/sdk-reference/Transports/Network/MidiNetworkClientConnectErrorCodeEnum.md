---
layout: sdk_reference_page
title: MidiNetworkClientConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when connecting to a remote Network MIDI 2.0 host
---

Returned in `MidiNetworkClientConnectResponse.ErrorCode`. The values from `NoReplyToInvitation` through `InvitationEndedByHost` say why a client's last invitation didn't open a session, and are reported in `MidiNetworkConfiguredClient.LastErrorCode` instead.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `InvalidJson` | `0x00000011` | The configuration sent to the service wasn't valid JSON |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `ClientId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `ClientId` wasn't a valid GUID |
| `InvalidOrMissingRemoteAddress` | `0x00000061` | The remote address was missing for a direct connection |
| `InvalidOrMissingRemotePort` | `0x00000062` | The remote port was missing for a direct connection |
| `RemotePortOutOfRange` | `0x00000063` | The remote port wasn't a valid port number |
| `InvalidOrMissingMatchCriteria` | `0x00000064` | Neither a device id nor a direct address was supplied |
| `InvalidNetworkProtocol` | `0x00000044` | That network protocol isn't supported. Only UDP is |
| `NoReplyToInvitation` | `0x00000071` | The remote host never answered the invitation. The service tries again |
| `InvitationNotApproved` | `0x00000072` | The remote host asked this PC to wait, and the request was never approved. The service tries again |
| `HostBusy` | `0x00000075` | The remote host said it has no room for another session. A Windows host also says this for about 10 seconds after a connection to it drops. The service tries again |
| `InvitationRefused` | `0x00000076` | The remote host's owner turned the connection down. The entry is `Failed` |
| `AuthenticationRequired` | `0x00000077` | The remote host wants a password or a user name, which Windows doesn't support yet. The entry is `Failed` |
| `InvitationEndedByHost` | `0x00000078` | The remote host ended the invitation for some other reason. The service tries again |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
