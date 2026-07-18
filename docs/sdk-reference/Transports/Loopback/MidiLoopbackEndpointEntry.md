---
layout: sdk_reference_page
title: MidiLoopbackEndpointEntry
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Information about one side of an active loopback endpoint pair
---

Represents one side (A or B) of an active transient loopback endpoint pair. Accessed through `MidiLoopbackEntry.EndpointA` and `MidiLoopbackEntry.EndpointB`.

## Properties

| Property | Description |
| -------- | ----------- |
| `EndpointDeviceId` | The full endpoint device id for this side of the loopback |
| `Name` | The name of this loopback endpoint |
| `Description` | The description of this loopback endpoint |
