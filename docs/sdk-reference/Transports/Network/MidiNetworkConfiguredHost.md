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
| `UmpEndpointName` | The UMP Endpoint Name remote devices see |
| `ProductInstanceId` | The Product Instance Id advertised for this host |
| `ServiceInstanceName` | The mDNS service instance name |
| `CreateMidi1Ports` | True if MIDI 1.0 ports are created alongside the UMP endpoints |

## Remarks

`ActualPort` is the port the host actually bound, which is what you want to display when the host was created with `UseAutomaticPortAllocation`.