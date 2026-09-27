---
layout: sdk_reference_page
title: MidiBasicLoopbackErrorCode
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: enum
description: Error codes returned by BasicLoopback transport operations
---

Error codes returned in `MidiBasicLoopbackCreationResponse`, `MidiBasicLoopbackRemovalResponse`, and `MidiBasicLoopbackUpdateResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The command sent to the service was not recognized |
| `InvalidJson` | `0x00000011` | The JSON configuration provided was not valid |
| `EndpointCreationFailed` | `0x00000021` | The service couldn't create the endpoint |
| `EndpointRemovalFailed` | `0x00000022` | The endpoint was found, but it couldn't be removed |
| `EndpointNotFound` | `0x00001065` | The endpoint couldn't be found |
| `InvalidOrMissingAssociationId` | `0x00000031` | The association id is missing or blank |
| `InvalidAssociationId` | `0x00000032` | The association id isn't a valid GUID |
| `InvalidOrMissingUniqueId` | `0x00000142` | The unique identifier is invalid or missing |
| `DuplicateUniqueId` | `0x00000141` | An endpoint with this unique id already exists |
| `InvalidOrMissingEndpointName` | `0x00000143` | The endpoint name is invalid or missing |
| `DuplicateEndpointName` | `0x00000144` | An endpoint with this name already exists |
| `UniqueIdTooLong` | `0x00000145` | The unique id is longer than 42 characters |
| `InvalidUniqueId` | `0x00000146` | The unique id has characters that can't be used in a device id |
| `FeedbackProtectionNotAvailable` | `0x00000041` | The basic loopback transport on this PC can't watch for feedback. See `MidiBasicLoopbackManager.IsFeedbackProtectionAvailable` |
| `ClientApiException` | `0x11002011` | An exception occurred in the client API |
| `InvalidArgument` | `0x11000055` | An invalid argument was provided |
