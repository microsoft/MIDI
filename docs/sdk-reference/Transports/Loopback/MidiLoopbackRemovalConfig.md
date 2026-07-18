---
layout: sdk_reference_page
title: MidiLoopbackRemovalConfig
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove a loopback endpoint pair
---

The configuration passed to the service when an application wants to remove a transient loopback endpoint pair.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiLoopbackRemovalConfig(associationId)` | Construct a removal config with the specified association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID used to uniquely identify the loopback pair to remove |
