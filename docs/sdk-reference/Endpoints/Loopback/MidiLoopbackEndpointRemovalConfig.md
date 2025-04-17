---
layout: sdk_reference_page
title: MidiLoopbackEndpointRemovalConfig
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Loopback
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Information supplied when removing a loopback endpoint pair
---

The configuration passed to the service when an application wants to remove a loopback endpoint pair.

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify the endpoint pair to remove |

## Methods

| Name | Description |
| -------- | ----------- |
| `MidiLoopbackEndpointRemovalConfig(associationId)` | Construct a removal config with the specified association id |
