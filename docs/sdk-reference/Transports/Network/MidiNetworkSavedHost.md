---
layout: sdk_reference_page
title: MidiNetworkSavedHost
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A Network MIDI 2.0 host saved in the configuration file
---

A Network MIDI 2.0 host saved in the configuration file. The service starts it every time it starts. `MidiNetworkTransportManager.GetSavedHosts` returns one of these for each saved host.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID that identifies this host. It's the same as `MidiNetworkConfiguredHost.HostId` when the host is running |
| `Name` | The UMP endpoint name the host announces |
| `ServiceInstanceName` | The name the host advertises on the network |
| `ProductInstanceId` | The product instance id the host announces |
| `IsEnabled` | False when the service shouldn't start the host |
| `CreateOnlyUmpEndpoints` | True when remote clients get only a UMP endpoint, with no MIDI 1.0 ports. This includes any change saved later with `MidiNetworkHostUpdateConfig` |
| `FallbackMidi1PortCount` | How many MIDI 1.0 ports a remote client gets when it doesn't describe itself. This includes any change saved later with `MidiNetworkHostUpdateConfig` |
| `UseAutomaticPortAllocation` | True when the host takes any free port |
| `ManuallyAssignedPort` | The port the host asks for. Empty when `UseAutomaticPortAllocation` is true |
| `AllowPortFallback` | True when the host may start on another port if its own is taken |
| `Advertise` | True when the host advertises itself on the network |
| `RemoteClientPolicy` | What the host does when a remote client it hasn't decided about asks to connect |
| `NetworkAdapterId` | The network adapter the host is limited to, or an empty GUID for every adapter. This includes any change saved later with `MidiNetworkHostUpdateConfig` |
| `NetworkAdapterName` | That adapter's name, from when it was chosen |
| `AllowNetworkAdapterFallback` | True when the host runs on every adapter while its own adapter is missing |
| `SendSpeedLimit` | How fast the host sends to each connected device. This includes any change saved later with `MidiNetworkHostUpdateConfig` |
| `ReduceSendSpeedAutomatically` | True when a connection sends more slowly while the device keeps asking for data again. This includes any change saved later with `MidiNetworkHostUpdateConfig` |
| `KnownRemoteClients` | Every saved allow and deny decision, as [MidiNetworkKnownRemoteClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkKnownRemoteClient/) objects |

## Remarks

This comes from the configuration file, not from the service. It tells you what the service starts the next time it starts, and it works even when the service isn't running. A missing value reads as the default the service uses.

To change a saved decision, start from `KnownRemoteClients`, make the change, and save a [MidiNetworkHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostKnownClientsConfig/) holding the whole list.

A decision saved without both a name and a product instance id is left out, because the service can't match it to a remote client.
