---
layout: sdk_reference_page
title: MidiRtpRemoteClientDisconnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: Error codes returned when ending one connection to an RTP-MIDI host on this PC
---

Returned in `MidiRtpRemoteClientDisconnectResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0` | No more information is available |
| `UnrecognizedCommand` | `2` | The RTP-MIDI transport in the service doesn't know this command, usually because it's older than this API |
| `InvalidOrMissingEntryIdentifier` | `3` | The request didn't say which host |
| `MalformedEntryIdentifier` | `4` | `HostId` isn't a valid GUID |
| `HostNotFound` | `5` | The running service has no host with this id |
| `TransportNotReady` | `7` | The transport hasn't finished starting. Try again in a moment |
| `InvalidOrMissingConnectionId` | `11` | `ConnectionId` is zero |
| `ConnectionNotFound` | `17` | The host is there, but it has no connection with that id. Most often the remote device had already left |
| `ServiceUnavailable` | `1000` | The request never reached the transport, because the MIDI service isn't running, or the RTP-MIDI transport isn't installed |
| `InvalidArgument` | `1002` | Your code passed a null configuration |
| `ClientApiException` | `1003` | Something went wrong in the client API |

## Remarks

The values from `1000` up come from the client API, not the service.
