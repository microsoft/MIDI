---
layout: sdk_reference_page
title: MidiDeclaredEndpointInfo
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Information declared by an endpoint during the MIDI 2.0 discovery process
---

The MIDI service fills this in during MIDI 2.0 endpoint discovery, from what the device says about itself. When you get one from `MidiEndpointDeviceInformation`, treat it as read-only. Changing it doesn't change the endpoint.

A virtual device application creates one to describe its device. See [`MidiVirtualDeviceCreationConfig`]({{ site.baseurl }}/sdk-reference/Transports/Virtual/MidiVirtualDeviceCreationConfig/).

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiDeclaredEndpointInfo()` | Creates an empty object |
| `MidiDeclaredEndpointInfo(name, productInstanceId, supportsMidi10Protocol, supportsMidi20Protocol, supportsReceivingJitterReductionTimestamps, supportsSendingJitterReductionTimestamps, hasStaticFunctionBlocks, declaredFunctionBlockCount, specificationVersionMajor, specificationVersionMinor)` | Creates an object with every value filled in |

## Properties

| Property | Description |
| --------------- | ----------- |
| `IsReadOnly` | True if you should treat this object as read-only |
| `Name` | The name the endpoint gave during discovery |
| `ProductInstanceId` | The endpoint's product instance id, which is usually its serial number |
| `SupportsMidi10Protocol` | True if the endpoint can use the MIDI 1.0 protocol in UMP. It says only that it can, not that it's set up to |
| `SupportsMidi20Protocol` | True if the endpoint can use the MIDI 2.0 protocol in UMP. It says only that it can, not that it's set up to |
| `SupportsReceivingJitterReductionTimestamps` | True if the endpoint can receive jitter reduction (JR) timestamps |
| `SupportsSendingJitterReductionTimestamps` | True if the endpoint can send jitter reduction timestamps |
| `HasStaticFunctionBlocks` | True if the device says its function blocks never change while it's connected |
| `DeclaredFunctionBlockCount` | How many function blocks the endpoint says it has |
| `SpecificationVersionMajor` | The major part of the UMP specification version the endpoint follows |
| `SpecificationVersionMinor` | The minor part of the UMP specification version the endpoint follows |
