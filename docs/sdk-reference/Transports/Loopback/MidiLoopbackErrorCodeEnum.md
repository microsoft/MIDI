---
layout: sdk_reference_page
title: MidiLoopbackErrorCode
namespace: Windows.Devices.Midi2.Transports.Loopback
type: enum
description: Error codes returned by Loopback transport operations
---

Error codes returned in `MidiLoopbackCreationResponse`, `MidiLoopbackRemovalResponse`, and `MidiLoopbackUpdateResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NoErrorInformationAvailable` | `0x00000000` | No additional error information is available |
| `UnrecognizedCommand` | `0x00000001` | The command sent to the service was not recognized |
| `InvalidJson` | `0x00000011` | The JSON configuration provided was not valid |
| `EndpointCreationFailed` | `0x00000021` | Endpoint creation failed in the service |
| `EndpointNotFound` | `0x00001065` | The specified endpoint could not be found |
| `InvalidOrMissingAssociationId` | `0x00000031` | The association id is invalid or missing |
| `InvalidOrMissingUniqueIdA` | `0x00000142` | The unique identifier for the A-side is invalid or missing |
| `DuplicateUniqueIdA` | `0x00000141` | An A-side endpoint with this unique id already exists |
| `InvalidOrMissingUniqueIdB` | `0x00000242` | The unique identifier for the B-side is invalid or missing |
| `DuplicateUniqueIdB` | `0x00000241` | A B-side endpoint with this unique id already exists |
| `InvalidOrMissingEndpointNameA` | `0x00000144` | The A-side endpoint name is invalid or missing |
| `DuplicateNameA` | `0x00000143` | An A-side endpoint with this name already exists |
| `InvalidOrMissingEndpointNameB` | `0x00000244` | The B-side endpoint name is invalid or missing |
| `DuplicateNameB` | `0x00000244` | A B-side endpoint with this name already exists |
| `ClientApiException` | `0x11000035` | An exception occurred in the client API |
| `InvalidArgument` | `0x11000055` | An invalid argument was provided |
| `ClientApiAllocationFailure` | `0x11000999` | Memory allocation failed in the client API |
