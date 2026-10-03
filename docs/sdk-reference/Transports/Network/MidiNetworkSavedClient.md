---
layout: sdk_reference_page
title: MidiNetworkSavedClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A Network MIDI 2.0 client saved in the configuration file
---

A Network MIDI 2.0 client saved in the configuration file. The service connects it every time it starts. `MidiNetworkTransportManager.GetSavedClients` returns one of these for each saved client.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client. It's the same as `MidiNetworkConfiguredClient.ClientId` when the client is set up in the service |
| `Comment` | The `Comment` saved with the entry, often the device's name. Empty when none was saved |
| `IsEnabled` | False when the service shouldn't connect the client |
| `CreateOnlyUmpEndpoints` | True when the connection gets only a UMP endpoint, with no MIDI 1.0 ports. This includes any change saved later with `MidiNetworkClientUpdateConfig` |
| `FallbackMidi1PortCount` | How many MIDI 1.0 ports the connection gets when the device doesn't describe itself. This includes any change saved later with `MidiNetworkClientUpdateConfig` |
| `UmpEndpointName` | The name this PC announces to the remote device. Empty means the machine name |
| `CustomEndpointName` | The name the customer chose for the endpoint. Empty when the endpoint uses the name the remote device announces |
| `MatchCriteria` | A [MidiNetworkClientMatchCriteria]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientMatchCriteria/) saying which remote device to connect to |
| `SendSpeedLimit` | How fast this PC sends to the remote device. This includes any change saved later with `MidiNetworkClientUpdateConfig` |
| `ReduceSendSpeedAutomatically` | True when the connection sends more slowly while the remote device keeps asking for data again. This includes any change saved later with `MidiNetworkClientUpdateConfig` |

## Remarks

This comes from the configuration file, not from the service. It tells you what the service connects the next time it starts, and it works even when the service isn't running.

`MatchCriteria` is a new copy each time you read it, so changing it changes nothing that's saved. It's also where a saved product instance id survives after a device changes its identity, for example in a firmware update.

To forget a saved client, pass a `MidiNetworkClientDisconnectConfig` to `MidiServiceTransportPluginConfigManager.SaveUpdate`.
