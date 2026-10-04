---
layout: sdk_reference_page
title: MidiNetworkConfiguredHost
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Information about a Network MIDI 2.0 host configured in the service
---

Returned by `MidiNetworkTransportManager.GetConfiguredHosts()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID that identifies this host entry |
| `IsEnabled` | True if this host is allowed to accept connections |
| `HasStarted` | True if the host is running and listening |
| `ActualPort` | The UDP port the host is using. It's a string, because that's what the network socket reports |
| `ActualAddress` | The local address the host is using |
| `ConfiguredPort` | The port the host was set up to use, or `auto`. Compare it with `ActualPort` to see whether the host got the port that was asked for |
| `AllowPortFallback` | True when the host was allowed to start on another port if the one it was set up with wasn't available |
| `UsedPortFallback` | True when that happened. The host works, but not on the port that was asked for, so it's worth showing people |
| `UmpEndpointName` | The UMP Endpoint Name remote devices see |
| `ProductInstanceId` | The Product Instance Id advertised for this host |
| `ServiceInstanceName` | The mDNS service instance name |
| `ActualServiceInstanceName` | The DNS-SD name really in use on the network. When two devices use the same name, the network renames one instead of refusing it, so this isn't always the name that was set up |
| `ServiceInstanceNameWasChanged` | True when that happened. The host works, but other devices see a different name than the one set up, so it's worth showing people |
| `CreateMidi1Ports` | True if MIDI 1.0 ports are created with the UMP endpoints |
| `RemoteClientPolicy` | What this host does when an unknown remote client asks to connect. See `MidiNetworkRemoteClientPolicy` |
| `NetworkAdapterId` | The network adapter the host is limited to, or an empty GUID for every adapter |
| `NetworkAdapterName` | That adapter's name, from when it was chosen |
| `AllowNetworkAdapterFallback` | True when the host runs on every adapter while its own adapter is missing |
| `IsNetworkAdapterMissing` | True when the host's adapter is missing. If `HasStarted` is also true, the host is running on every adapter until the adapter is back. If not, the host is waiting, and starts by itself when the adapter is back |
| `UsedNetworkAdapterFallback` | True when the host is running on every adapter because its own adapter is missing |
| `SendSpeedLimit` | How fast the host is set up to send to each connected device. See [MidiNetworkSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSendSpeedLimitEnum/) |
| `ReduceSendSpeedAutomatically` | True when a connection sends more slowly while the device keeps asking for data again. Each connection's speed right now is in `MidiNetworkHostConnection.CurrentSendSpeedLimit` |
| `RemoteClientSettings` | The remote clients this host sends to at a speed of their own instead of the host's, as [MidiNetworkRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkRemoteClientSettings/) entries. This is the list the running service holds, whether or not the clients are connected |
| `Connections` | The remote clients that have reached this host right now, including clients waiting for approval |

## Remarks

`ActualPort` is the port the host is really using. Show it when the host was created with `UseAutomaticPortAllocation`.

A host whose adapter is missing is worth showing people. It's either not running at all, or running on networks it was set up to stay off.

To change a client's own speed, start from `RemoteClientSettings`, change it, and send a [MidiNetworkHostRemoteClientSettingsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostRemoteClientSettingsConfig/) holding the whole list.

`Connections` is a copy taken when you asked, not a list that updates itself. Call `GetConfiguredHosts()` again to refresh it.