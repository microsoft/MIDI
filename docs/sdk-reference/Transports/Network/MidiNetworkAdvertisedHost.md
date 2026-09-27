---
layout: sdk_reference_page
title: MidiNetworkAdvertisedHost
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A Network MIDI 2.0 host discovered on the network over mDNS
---

Describes a host advertised on the local network. Get these from `MidiNetworkAdvertisedHostWatcher` or `MidiNetworkTransportManager.GetAdvertisedHosts()`.

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
| `TextAttributes` | Everything the mDNS TXT record had in it, as a map. If a device uses a key that's newer than this API, you can still read it here without an API update |
| `IPAddresses` | The IP addresses the host advertised. There may be more than one |
| `IPv4Addresses` | Just the IPv4 addresses, from the A records |
| `IPv6Addresses` | Just the IPv6 addresses, from the AAAA records |
| `LastSeenTime` | When this host was last heard, so your app can show how out of date an entry is |

## Remarks

Use `DeviceId` to connect when you can. The MIDI 2.0 specification says to use the `UmpEndpointName` and `ProductInstanceId` pair to recognize a device and bring back its settings when it reconnects. Addresses and ports change, so they don't identify a device.