---
layout: sdk_reference_page
title: MidiDeclaredEndpointInfo
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Information declared by an endpoint during the MIDI 2.0 discovery process
---

This information is populated by the Windows Service during the MIDI 2.0 endpoint discovery process.

## Struct Fields

| Field | Description |
| --------------- | ----------- |
| `Name` | The name as provided by endpoint discovery |
| `ProductInstanceId` | Product Instance Id (a type of unique identifier) from the endpoint |
| `SupportsMidi10Protocol` | True if the endpoint is capable of using the MIDI 1.0 protocol in UMP. This does not indicate if it is configured to do so, only that it is capable. |
| `SupportsMidi20Protocol` | True if the endpoint is capable of using the MIDI 2.0 protocol in UMP. This does not indicate if it is configured to do so, only that it is capable. |
| `SupportsReceivingJitterReductionTimestamps` | True if the endpoint supports receiving JR timestamps. Applications should not send or process JR timestamps. |
| `SupportsSendingJitterReductionTimestamps` | True if the endpoint supports receiving JR timestamps. Applications should not send or process JR timestamps. |
| `HasStaticFunctionBlocks` | True if this device declares that the function blocks are static for the lifetime of its connection to Windows. |
| `DeclaredFunctionBlockCount` | This is the number of function blocks the endpoint claims to have. Note that this should not be used as an upper bound when iterating through the collection of function blocks, because this is not the count of function blocks actually received by the Windows service. |
| `SpecificationVersionMajor` | UMP specification version as described in the MIDI 2.0 UMP spec. |
| `SpecificationVersionMinor` | UMP specification version as described in the MIDI 2.0 UMP spec. |
