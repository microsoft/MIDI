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
| `HostId` | The GUID which identifies this host entry |
| `IsEnabled` | True if this host is permitted to accept connections |
| `HasStarted` | True if the host is running and listening |
| `ActualPort` | The UDP port the host is bound to. A string, because that is what the underlying socket reports |
| `ActualAddress` | The local address the host is bound to |
| `ConfiguredPort` | What the host was configured to use, or `auto`. Compare with `ActualPort` to see whether the customer got the port they asked for |
| `AllowPortFallback` | True when the host was permitted to start on an allocated port if the configured one was unavailable |
| `UsedPortFallback` | True when that happened. The host is working, but not where the customer asked it to be, so this is worth surfacing |
| `UmpEndpointName` | The UMP Endpoint Name remote devices see |
| `ProductInstanceId` | The Product Instance Id advertised for this host |
| `ServiceInstanceName` | The mDNS service instance name |
| `ActualServiceInstanceName` | The DNS-SD instance label actually on the network. A responder renames a colliding label rather than refusing it, so this is not always the configured name |
| `ServiceInstanceNameWasChanged` | True when that happened. The host works, but other devices see a different name than the one configured, so this is worth showing |
| `CreateMidi1Ports` | True if MIDI 1.0 ports are created alongside the UMP endpoints |
| `RemoteClientPolicy` | What this host does when an unknown remote client requests a connection. See `MidiNetworkRemoteClientPolicy`. |
| `Connections` | The current remote clients that have reached this host, including clients waiting for approval. |

## Remarks

`ActualPort` is the port the host actually bound, which is what you want to display when the host was created with `UseAutomaticPortAllocation`.

`Connections` is a snapshot, not a live collection. Poll `GetConfiguredHosts()` to refresh.