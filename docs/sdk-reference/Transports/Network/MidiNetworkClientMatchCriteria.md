---
layout: sdk_reference_page
title: MidiNetworkClientMatchCriteria
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Describes how to locate the remote host a client should connect to
---

Supplies either a discovered device id or a direct address. Used by `MidiNetworkClientConnectConfig`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkClientMatchCriteria()` | Create empty criteria |

## Properties

| Property | Description |
| -------- | ----------- |
| `DeviceId` | The device id of a host discovered over mDNS, as reported by `MidiNetworkAdvertisedHost.DeviceId` |
| `ProductInstanceId` | The device's own product instance id, per the specification's recall pair |
| `UmpEndpointName` | The device's own UMP Endpoint Name, the other half of that pair |
| `DirectHostNameOrIPAddress` | Host name or IP address of the remote host, for a direct connection |
| `DirectPort` | UDP port of the remote host, for a direct connection |

Supplying `ProductInstanceId` and `UmpEndpointName` as well as `DeviceId` is worth doing. The DNS-SD instance label behind `DeviceId` is a name, not an identity: a responder renames it to resolve a collision, and a customer or a firmware update can change it. The identity pair lets a reconnect still find the device after that happens.

## Methods

| Method | Description |
| -------- | ----------- |
| `GetConfigJson()` | Returns the JSON fragment this object produces, for writing to the configuration file |

## Remarks

Set `DeviceId` for a discovered host, or `DirectHostNameOrIPAddress` and `DirectPort` for a direct one. The choice determines how the service behaves when the host is unreachable: a discovered host is retried whenever it advertises again, whereas a direct address is tried once and then marked unavailable. See the [namespace overview](index) for the full table.