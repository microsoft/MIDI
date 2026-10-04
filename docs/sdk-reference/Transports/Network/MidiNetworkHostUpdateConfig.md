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
| `NetworkAdapterId` | Moves the host to another network adapter, or to every adapter with an empty GUID. The host restarts on it, which ends its connections. Setting it also fills in `NetworkAdapterName` |
| `NetworkAdapterName` | The adapter's name. It's only shown to people |
| `AllowNetworkAdapterFallback` | What the host does while its adapter is missing: true to run on every adapter until the adapter is back, false to wait for it |
| `SendSpeedLimit` | How fast the host sends to each connected device. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | Whether a connection sends more slowly while the device keeps asking for data again |

## Remarks

Only the properties you set are changed. Everything else about the host stays as it is.

The host keeps running, unless you change `NetworkAdapterId`. Only the settings that can change on a running host take effect right away.

`SendSpeedLimit` and `ReduceSendSpeedAutomatically` apply straight away, to the connections that are already up as well as to new ones, without disconnecting anything.

`FallbackMidi1PortCount` applies to connections that are already running. Ports are added or removed without interrupting the session. `CreateMidi1Ports` applies the next time a remote device connects, because whether an endpoint has MIDI 1.0 ports at all is decided when the endpoint is built.

Neither setting does anything for a device that describes itself with function blocks. Those come first, and the service builds one port per group from them. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
