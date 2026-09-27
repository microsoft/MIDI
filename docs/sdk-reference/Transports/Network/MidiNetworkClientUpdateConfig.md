---
layout: sdk_reference_page
title: MidiNetworkClientUpdateConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to change settings on an existing Network MIDI 2.0 client connection
---

Pass to `MidiNetworkTransportManager.UpdateNetworkClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientUpdateConfig()` | Creates an empty configuration |
| `MidiNetworkClientUpdateConfig(clientId)` | Creates a configuration for this client |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry to change |
| `CreateMidi1Ports` | Whether this connection gets MIDI 1.0 ports |
| `FallbackMidi1PortCount` | How many source and destination ports to create when the remote host declares no function blocks. 1 to 16 |

## Remarks

The connection stays up. Only the settings that can change on a running connection take effect right away.

`FallbackMidi1PortCount` applies to an endpoint that's already running. Ports are added or removed without disconnecting. `CreateMidi1Ports` is saved for the next connection, because whether an endpoint has MIDI 1.0 ports at all is decided when the endpoint is built. To use it now, disconnect and connect again.

Neither setting does anything for a remote host that describes itself with function blocks. Those come first, and the service builds one port per group from them. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
