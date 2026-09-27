---
layout: sdk_reference_page
title: MidiNetworkHostUpdateConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to change settings on an existing Network MIDI 2.0 host
---

Pass to `MidiNetworkTransportManager.UpdateNetworkHostAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostUpdateConfig()` | Creates an empty configuration |
| `MidiNetworkHostUpdateConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry to change |
| `CreateMidi1Ports` | Whether connected devices get MIDI 1.0 ports |
| `FallbackMidi1PortCount` | How many source and destination ports to create for a device that declares no function blocks. 1 to 16 |

## Remarks

The host keeps running. Only the settings that can change on a running host take effect right away.

`FallbackMidi1PortCount` applies to connections that are already running. Ports are added or removed without interrupting the session. `CreateMidi1Ports` applies the next time a remote device connects, because whether an endpoint has MIDI 1.0 ports at all is decided when the endpoint is built.

Neither setting does anything for a device that describes itself with function blocks. Those come first, and the service builds one port per group from them. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
