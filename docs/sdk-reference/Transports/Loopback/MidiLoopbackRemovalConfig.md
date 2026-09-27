---
layout: sdk_reference_page
title: MidiLoopbackRemovalConfig
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove a loopback endpoint pair
---

The configuration your app sends to the service to remove a temporary loopback endpoint pair.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiLoopbackRemovalConfig(associationId)` | Creates a removal configuration for the loopback pair with this association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID of the loopback pair to remove |
