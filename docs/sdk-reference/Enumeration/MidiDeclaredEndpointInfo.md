---
layout: sdk_reference_page
title: MidiDeclaredEndpointInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Information declared by an endpoint during the MIDI 2.0 discovery process
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process. When retrieved from a `MidiEndpointDeviceInformation` object, it is read-only.

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if this object should be treated as read-only |
| `Name` | The name as provided by endpoint discovery |
| `ProductInstanceId` | Product Instance Id (a type of unique identifier) from the endpoint |
| `SupportsMidi10Protocol` | True if the endpoint is capable of using the MIDI 1.0 protocol in UMP. This does not indicate if it is configured to do so, only that it is capable. |
| `SupportsMidi20Protocol` | True if the endpoint is capable of using the MIDI 2.0 protocol in UMP. This does not indicate if it is configured to do so, only that it is capable. |
| `SupportsReceivingJitterReductionTimestamps` | True if the endpoint supports receiving JR timestamps. |
| `SupportsSendingJitterReductionTimestamps` | True if the endpoint supports sending JR timestamps. |
| `HasStaticFunctionBlocks` | True if this device declares that the function blocks are static for the lifetime of its connection to Windows. |
| `DeclaredFunctionBlockCount` | The number of function blocks the endpoint claims to have. |
| `SpecificationVersionMajor` | UMP specification version major number as described in the MIDI 2.0 UMP spec. |
| `SpecificationVersionMinor` | UMP specification version minor number as described in the MIDI 2.0 UMP spec. |
