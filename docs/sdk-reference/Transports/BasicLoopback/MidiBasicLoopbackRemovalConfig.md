---
layout: sdk_reference_page
title: MidiBasicLoopbackRemovalConfig
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove a basic MIDI 1.0-style loopback endpoint
---

The configuration your app sends to the service to remove a temporary basic loopback endpoint.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBasicLoopbackRemovalConfig(associationId)` | Creates a removal configuration for the loopback with this association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID of the loopback to remove |
