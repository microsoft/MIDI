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
| `MidiNetworkHostUpdateConfig()` | Create an empty config |
| `MidiNetworkHostUpdateConfig(hostId)` | Create a config for the specified host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry to change |
| `CreateMidi1Ports` | Whether connected devices get classic MIDI 1.0 ports |
| `FallbackMidi1PortCount` | Source and destination ports to create for a device which declares no function blocks. 1 to 16 |

## Remarks

The host keeps running. Only the settings which can take effect on a live host do so.

`FallbackMidi1PortCount` applies to connections which are already up: ports are added or removed without the session being interrupted. `CreateMidi1Ports` applies the next time a remote connects, because whether an endpoint has MIDI 1.0 ports at all is settled when the endpoint is built.

Neither setting has any effect on a device which describes itself with function blocks. Those take precedence, and the service builds one port per group from them. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
