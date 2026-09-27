---
layout: sdk_reference_page
title: MidiLoopbackEndpointEntry
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: Information about one side of an active loopback endpoint pair
---

One side, A or B, of a temporary loopback endpoint pair that exists right now. Get it from `MidiLoopbackEntry.EndpointA` or `MidiLoopbackEntry.EndpointB`.

## Properties

| Property | Description |
| -------- | ----------- |
| `EndpointDeviceId` | The full endpoint device id of this side of the loopback |
| `Name` | The name of this loopback endpoint |
| `Description` | The description of this loopback endpoint |
| `ImageFileName` | The file name of the endpoint's picture in the shared endpoint assets folder, or empty if there's none |
