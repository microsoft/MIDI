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
| `MidiNetworkClientUpdateConfig()` | Create an empty config |
| `MidiNetworkClientUpdateConfig(clientId)` | Create a config for the specified client |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry to change |
| `CreateMidi1Ports` | Whether this connection gets classic MIDI 1.0 ports |
| `FallbackMidi1PortCount` | Source and destination ports to create when the remote host declares no function blocks. 1 to 16 |

## Remarks

The connection stays up. Only the settings which can take effect on a live connection do so.

`FallbackMidi1PortCount` is applied to an endpoint which is already up: ports are added or removed without disconnecting. `CreateMidi1Ports` is recorded for the next connection, because whether an endpoint has MIDI 1.0 ports at all is settled when the endpoint is built. Disconnect and reconnect to act on it now.

Neither setting has any effect on a remote host which describes itself with function blocks. Those take precedence, and the service builds one port per group from them. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
