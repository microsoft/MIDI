---
layout: sdk_reference_page
title: MidiRtpHostRemovalErrorCode
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: Error codes returned when removing an RTP-MIDI host
---

Returned in `MidiRtpHostRemovalResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0` | No more information is available |
| `UnrecognizedCommand` | `2` | The RTP-MIDI transport in the service doesn't know this command, usually because it's older than this API |
| `InvalidOrMissingEntryIdentifier` | `3` | The request didn't say which host |
| `MalformedEntryIdentifier` | `4` | `HostId` isn't a valid GUID |
| `HostNotFound` | `5` | The running service has no host with this id |
| `TransportNotReady` | `7` | The transport hasn't finished starting. Try again in a moment |
| `ServiceUnavailable` | `1000` | The request never reached the transport, because the MIDI service isn't running, or the RTP-MIDI transport isn't installed |
| `InvalidArgument` | `1002` | Your code passed a null configuration |
| `ClientApiException` | `1003` | Something went wrong in the client API |

## Remarks

The values from `1000` up come from the client API, not the service.
