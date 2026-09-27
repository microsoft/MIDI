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
| `UnrecognizedCommand` | `0x00000001` | The service didn't recognize the command |
| `InvalidJson` | `0x00000011` | The configuration sent to the service wasn't valid JSON |
| `HostCreationFailed` | `0x00000021` | The service couldn't create the host |
| `InvalidOrMissingEntryIdentifier` | `0x00000031` | `HostId` was missing |
| `MalformedEntryIdentifier` | `0x00000032` | `HostId` wasn't a valid GUID |
| `InvalidOrMissingEndpointName` | `0x00000041` | `Name` was missing or blank |
| `EndpointNameTooLong` | `0x00000045` | `Name` is longer than the 98 bytes the MIDI 2.0 specification allows |
| `InvalidOrMissingProductInstanceId` | `0x00000042` | `ProductInstanceId` was missing or blank |
| `ProductInstanceIdTooLong` | `0x00000046` | `ProductInstanceId` is longer than the 42 bytes the specification allows |
| `ServiceInstanceNameInUse` | `0x00000043` | Another host is already using this `ServiceInstanceName` |
| `ServiceInstanceNameTooLong` | `0x0000004A` | `ServiceInstanceName` is longer than mDNS allows |
| `InvalidNetworkProtocol` | `0x00000044` | That network protocol isn't supported. Only UDP is |
| `InvalidNetworkPort` | `0x00000048` | The port is outside the range of valid UDP ports |
| `NetworkPortInUse` | `0x00000049` | The port is already in use on this PC |
| `InvalidOrMissingCredentialIdentifier` | `0x00000051` | Authentication was asked for, but no credential id was supplied |
| `MalformedCredentialIdentifier` | `0x00000052` | The credential id isn't valid |
| `AuthenticationNotImplemented` | `0x00000053` | Authentication isn't built yet. Set up the host with no authentication |
| `InvalidArgument` | `0x11000055` | Your code passed an argument that isn't valid |
| `ClientApiException` | `0x11002011` | An exception happened in the client API |
| `TimedOutWaitingForHostToStart` | `0x110005B4` | The service accepted the host, but the host didn't start in time |

## Remarks

`TimedOutWaitingForHostToStart` comes from the client API, not the service. `CreateNetworkHostAsync` waits for the host to start, and reports this if it doesn't, instead of reporting success that isn't true.
