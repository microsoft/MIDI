---
layout: sdk_reference_page
title: MidiRtpHostCreationErrorCode
namespace: Windows.Devices.Midi2.Transports.Rtp
type: enum
description: Error codes returned when creating an RTP-MIDI host
---

Returned in `MidiRtpHostCreationResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0` | No more information is available |
| `InvalidJson` | `1` | The service couldn't read the configuration it was sent |
| `MalformedEntryIdentifier` | `4` | `HostId` isn't a valid GUID |
| `InvalidEntry` | `6` | The service couldn't read the host entry |
| `NameTooLong` | `9` | `Name` or `ServiceInstanceName` is longer than 63 bytes in UTF-8 |
| `InvalidPort` | `10` | `ManuallyAssignedPort` isn't between 1024 and 65534 |
| `InvalidName` | `12` | A name couldn't be used. A name that's advertised can't have a period in it |
| `NetworkAdapterNotAvailable` | `18` | The host is limited to a network adapter that's missing, and isn't allowed to use the others. The host is still created, and starts by itself when the adapter is back |
| `ServiceUnavailable` | `1000` | The request never reached the transport, because the MIDI service isn't running, or the RTP-MIDI transport isn't installed |
| `TimedOutWaitingForHostToStart` | `1001` | The service accepted the host, but the host didn't start in time |
| `InvalidArgument` | `1002` | Your code passed a null configuration |
| `ClientApiException` | `1003` | Something went wrong in the client API |

## Remarks

The values from `1000` up come from the client API, not the service. `CreateRtpHostAsync` waits for the host to start, and reports `TimedOutWaitingForHostToStart` if it doesn't, instead of reporting success that isn't true yet.
