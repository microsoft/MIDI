---
layout: sdk_reference_page
title: MidiNetworkKnownRemoteClient
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A remote client a Network MIDI 2.0 host has already been told to allow or deny
---

Held in [MidiNetworkHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostKnownClientsConfig/).`KnownClients`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkKnownRemoteClient()` | Create an empty entry |
| `MidiNetworkKnownRemoteClient(remoteClientName, remoteClientProductInstanceId, isAllowed)` | Create an entry for the named client |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The UMP endpoint name the remote client announces |
| `RemoteClientProductInstanceId` | The product instance id the remote client announces |
| `IsAllowed` | True to let the client connect without asking again, false to turn it away |

## Remarks

A remote client is identified by the name and product instance id together, compared without case. Neither half identifies it on its own, so both are needed to recognize the same client again. An entry with both empty is ignored when the config is saved.
