---
layout: sdk_reference_page
title: MidiBasicLoopbackRemovalConfig
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove a basic MIDI 1.0-style loopback endpoint
---

This is the configuration sent to the service when an application wants to remove a transient basic loopback endpoint.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBasicLoopbackRemovalConfig(associationId)` | Create a removal config for the loopback with the specified association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID identifying the loopback to remove |
