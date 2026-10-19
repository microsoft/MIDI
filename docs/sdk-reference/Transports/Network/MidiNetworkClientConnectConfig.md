---
layout: sdk_reference_page
title: MidiNetworkClientConnectConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to connect to a remote Network MIDI 2.0 host
---

Pass to `MidiNetworkTransportManager.ConnectNetworkClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientConnectConfig()` | Creates an empty configuration |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client entry. It's used to disconnect later, and to find the entry in the configuration |
| `Comment` | An optional comment saved with the entry in the configuration. The service doesn't use it |
| `CreateOnlyUmpEndpoints` | When true, only UMP endpoints are created. When false, MIDI 1.0 ports are created with them |
| `FallbackMidi1PortCount` | How many source and destination ports to create when the remote host declares no function blocks. 1 to 16, and 1 by default. Ignored when the host does describe itself, when `CreateOnlyUmpEndpoints` is true, and when `FallbackMidi1PortGroups` isn't empty |
| `FallbackMidi1PortGroups` | The groups to create MIDI 1.0 ports for when the remote host declares no function blocks, and what to call them. A list of [MidiNetworkMidi1PortGroup]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkMidi1PortGroup/), empty by default. Used instead of `FallbackMidi1PortCount` when it isn't empty. Added in version 2 of `MidiTransportsNetworkApiContract` |
| `UmpEndpointName` | The UMP Endpoint Name to use for this PC's end of the connection. Leave it empty to use the computer name. Whatever you choose is kept when the connection is read back after a restart, because the remote device knows this PC by this name and its product instance id |
| `CustomEndpointName` | The name the person chose for the MIDI endpoint this connection creates. It's applied before the endpoint is turned on, so the endpoint and its MIDI 1.0 ports never appear under the remote device's own name first. Leave it empty to use the name the remote device announces |
| `MatchCriteria` | A `MidiNetworkClientMatchCriteria` that says which remote host to connect to |
| `SendSpeedLimit` | How fast this PC sends to the remote host. `Unlimited` by default. Choose a slower speed for a device that loses data when a lot of it arrives at once, like a long SysEx dump. A lone message is never delayed. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | When true, the connection sends more slowly while the remote host keeps asking for data again, and speeds back up to `SendSpeedLimit` once it stops. False by default. See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#sending-speed) |
| `SkipInBetweenValuesToCatchUp` | When true, and messages pile up behind `SendSpeedLimit`, the connection skips some in-between steps of long control change, pressure and pitch bend moves so the remote host catches up. True by default. Set it to false for a device that needs every message, however late. Added in version 2 of `MidiTransportsNetworkApiContract` |

## Remarks

Calling `ConnectNetworkClientAsync` with a `ClientId` that already exists doesn't create a copy. It starts the existing entry trying again right away, which is how you bring back an entry marked `Failed`.