---
layout: sdk_reference_page
title: MidiNetworkClientMatchCriteria
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Describes how to locate the remote host a client should connect to
---

Holds either the device id of a discovered host, or a direct address. Used by `MidiNetworkClientConnectConfig`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientMatchCriteria()` | Creates empty criteria |

## Properties

| Property | Description |
| -------- | ----------- |
| `DeviceId` | The device id of a host found over mDNS, as reported by `MidiNetworkAdvertisedHost.DeviceId` |
| `ProductInstanceId` | The device's own product instance id. The MIDI 2.0 specification uses this and the UMP Endpoint Name together to recognize a device |
| `UmpEndpointName` | The device's own UMP Endpoint Name, the other half of that pair |
| `DirectHostNameOrIPAddress` | The host name or IP address of the remote host, for a direct connection |
| `DirectPort` | The UDP port of the remote host, for a direct connection |

It's worth setting `ProductInstanceId` and `UmpEndpointName` as well as `DeviceId`. The DNS-SD name behind `DeviceId` is just a name, and it can change. The network renames it when two devices use the same name, and a person or a firmware update can change it too. The id pair still finds the device when it reconnects after that happens.

## Methods

| Method | Description |
| -------- | ----------- |
| `GetConfigJson()` | Returns the JSON for this object, for saving in the configuration |

## Remarks

Set `DeviceId` for a discovered host, or `DirectHostNameOrIPAddress` and `DirectPort` for a direct one. Your choice decides what the service does when the host can't be reached. A discovered host is tried again whenever it advertises. A direct one is tried again after the retry interval, and a host name is looked up again each time. See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Network/) for the full table.