---
layout: sdk_reference_page
title: MidiRtpClientConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: Error codes returned when connecting or reconnecting an RTP-MIDI client
---

Returned in `MidiRtpClientConnectResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0` | No more information is available |
| `InvalidJson` | `1` | The service couldn't read the configuration it was sent |
| `UnrecognizedCommand` | `2` | From `ReconnectRtpClientAsync`, when the RTP-MIDI transport in the service doesn't know this command, usually because it's older than this API |
| `InvalidOrMissingEntryIdentifier` | `3` | The request didn't say which client |
| `MalformedEntryIdentifier` | `4` | The client id isn't a valid GUID |
| `ClientNotFound` | `5` | From `ReconnectRtpClientAsync`, when the running service has no client with this id |
| `InvalidEntry` | `6` | The service couldn't read the client entry |
| `TransportNotReady` | `7` | The transport hasn't finished starting. Try again in a moment |
| `InvalidOrMissingMatchCriteria` | `8` | `MatchCriteria` needs either an advertised name, or an address and a port, but not both |
| `NameTooLong` | `9` | `Name` or the advertised name is longer than 63 bytes in UTF-8, or `CustomEndpointName` or the address is longer than 255 characters |
| `InvalidPort` | `10` | The direct port isn't between 1024 and 65534 |
| `InvalidName` | `12` | `Name` couldn't be used |
| `ServiceUnavailable` | `1000` | The request never reached the transport, because the MIDI service isn't running, or the RTP-MIDI transport isn't installed |
| `InvalidArgument` | `1002` | Your code passed a null configuration |
| `ClientApiException` | `1003` | Something went wrong in the client API |

## Remarks

The values from `1000` up come from the client API, not the service.

These codes are about the request. A remote device that can't be found, doesn't answer, or turns the connection down isn't an error here, because connecting happens after the request returns. Use `MidiRtpConfiguredClient.EntryState` and `LastErrorCode` for those.
