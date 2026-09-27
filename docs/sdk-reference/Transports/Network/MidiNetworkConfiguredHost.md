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
| `Connections` | The remote clients that have reached this host right now, including clients waiting for approval |

## Remarks

`ActualPort` is the port the host is really using. Show it when the host was created with `UseAutomaticPortAllocation`.

`Connections` is a copy taken when you asked, not a list that updates itself. Call `GetConfiguredHosts()` again to refresh it.