---
layout: sdk_reference_page
title: MidiNetworkHostRemovalConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove a Network MIDI 2.0 host
---

Pass to `MidiNetworkTransportManager.RemoveNetworkHostAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostRemovalConfig()` | Creates an empty configuration |
| `MidiNetworkHostRemovalConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host to remove |

## Remarks

Removing a host disconnects every remote client connected to it, and frees its UDP port and mDNS advertisement. Its clients' own sending speeds go with it.