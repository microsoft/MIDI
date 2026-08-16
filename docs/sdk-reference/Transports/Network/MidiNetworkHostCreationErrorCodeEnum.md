---
layout: sdk_reference_page
title: MidiNetworkHostCreationErrorCode
namespace: Windows.Devices.Midi2.Transports.Network
type: enum
description: Error codes returned when creating a Network MIDI 2.0 host
---

Returned in `MidiNetworkHostCreationResponse.ErrorCode`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The command sent to the service was not recognized |
| `InvalidJson` | `0x00000011` | The configuration sent to the service was not valid JSON |
| `HostCreationFailed` | `0x00000021` | The host could not be created in the service |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `HostId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `HostId` was not a valid GUID |
| `InvalidOrMissingEndpointName` | `0x00000041` | `Name` was missing or blank |
| `EndpointNameTooLong` | `0x00000045` | `Name` is longer than the 98 bytes the MIDI 2.0 specification allows |
| `InvalidOrMissingProductInstanceId` | `0x00000042` | `ProductInstanceId` was missing or blank |
| `ProductInstanceIdTooLong` | `0x00000046` | `ProductInstanceId` is longer than the 42 bytes the specification allows |
| `ServiceInstanceNameInUse` | `0x00000043` | Another host is already using this `ServiceInstanceName` |
| `InvalidNetworkProtocol` | `0x00000044` | The network protocol specified is not supported. Only UDP is |
| `InvalidOrMissingCredentialIdentifier` | `0x00000051` | Authentication was requested but no credential identifier was supplied |
| `MalformedCredentialIdentifier` | `0x00000052` | The supplied credential identifier is not valid |
| `AuthenticationNotImplemented` | `0x00000053` | Authentication is not yet implemented. Configure the host for no authentication |
| `ForwardErrorCorrectionPacketCountOutOfRange` | `0x00000081` | The transport-wide forward error correction packet count is out of range |
| `RetransmitBufferSizeOutOfRange` | `0x00000082` | The transport-wide retransmit buffer size is out of range |
| `PingIntervalOutOfRange` | `0x00000083` | The transport-wide outbound ping interval is out of range |
| `MaxHostConnectionsOutOfRange` | `0x00000084` | The transport-wide maximum host connection count is out of range |
| `InvitationPendingTimeoutOutOfRange` | `0x00000085` | The transport-wide invitation pending timeout is out of range |
| `ScanIntervalOutOfRange` | `0x00000086` | The transport-wide direct connection scan interval is out of range |
| `InvalidArgument` | `0x11000055` | An invalid argument was supplied by the caller |
| `ClientApiException` | `0x11002011` | An exception occurred in the client API |
| `T` | `i` | m |
| `0` | `x` | 1 |
| `T` | `h` | e |

## Remarks

`TimedOutWaitingForHostToStart` is raised by the client API rather than the service. `CreateNetworkHostAsync` waits for the host to actually come up, and reports this rather than a false success if it does not.
