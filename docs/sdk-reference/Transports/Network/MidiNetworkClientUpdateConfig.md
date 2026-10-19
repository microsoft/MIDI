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
| `SendSpeedLimit` | How fast this PC sends to the remote host. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | Whether the connection sends more slowly while the remote host keeps asking for data again |
| `SkipInBetweenValuesToCatchUp` | Whether the connection skips some in-between values to catch up when messages pile up behind `SendSpeedLimit`. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `FallbackMidi1PortGroups` | The groups to create MIDI 1.0 ports for when the remote host declares no function blocks, and what to call them. A list of [MidiNetworkMidi1PortGroup]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkMidi1PortGroup/). Used instead of `FallbackMidi1PortCount` when it isn't empty. Added in version 2 of `MidiTransportsNetworkApiContract` |

## Remarks

The connection stays up. Only the settings that can change on a running connection take effect right away.

`SendSpeedLimit`, `ReduceSendSpeedAutomatically` and `SkipInBetweenValuesToCatchUp` are only changed when you set them, so a speed chosen somewhere else is left alone when you change only the ports. All three apply straight away, without disconnecting.

`CreateMidi1Ports` and `FallbackMidi1PortCount` are always sent. When you only mean to change something else, set them to what the entry already has, which [MidiNetworkSavedClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedClient/) reports.

`FallbackMidi1PortGroups` is only sent once you change the list, so ports named somewhere else are kept when you change something else. Build the whole list, because it replaces the one the entry has. Clearing it sends an empty list, which goes back to `FallbackMidi1PortCount`.

`FallbackMidi1PortCount` and `FallbackMidi1PortGroups` apply to an endpoint that's already running. Ports are added, removed or renamed without disconnecting. `CreateMidi1Ports` is saved for the next connection, because whether an endpoint has MIDI 1.0 ports at all is decided when the endpoint is built. To use it now, disconnect and connect again.

None of these do anything for a remote host that describes itself with function blocks. Those come first, and the service builds one port per group from them. A name given in `FallbackMidi1PortGroups` still applies to a port on the same group. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
