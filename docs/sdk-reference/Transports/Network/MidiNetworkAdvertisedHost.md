---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHost
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A Network MIDI 2.0 host discovered on the network over mDNS
---

Describes a host advertised on the local network. Obtained from `MidiNetworkAdvertisedHostWatcher` or `MidiNetworkTransportManager.GetAdvertisedHosts()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `DeviceId` | The device id for this advertised host. Use this as `MidiNetworkClientMatchCriteria.DeviceId` to connect to it |
| `DeviceName` | The name reported by device enumeration |
| `FullName` | The full DNS-SD name |
| `ServiceInstanceName` | The DNS-SD service instance name |
| `ServiceType` | The DNS-SD service type |
| `HostName` | The DNS host name, for example `somemachine.local` |
| `Port` | The UDP port the remote host is listening on |
| `Domain` | The DNS-SD domain |
| `UmpEndpointName` | The UMP Endpoint Name advertised by the host |
| `ProductInstanceId` | The Product Instance Id advertised by the host |
| `TextAttributes` | Everything the mDNS TXT record carried, as a map. A device using a key this SDK predates is still readable through this without an SDK update |
| `IPAddresses` | The IP addresses the host advertised. May contain more than one |
| `IPv4Addresses` | Just the IPv4 addresses, from the A records |
| `IPv6Addresses` | Just the IPv6 addresses, from the AAAA records |
| `LastSeenTime` | When this host was last heard from, for a UI which shows how stale an entry is |

## Remarks

Prefer `DeviceId` when connecting. The MIDI 2.0 specification nominates the `UmpEndpointName` and `ProductInstanceId` pair as the identity used to recall a device's settings across reconnects; addresses and ports change and are not identity.