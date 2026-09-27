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
| `MidiNetworkKnownRemoteClient()` | Creates an empty entry |
| `MidiNetworkKnownRemoteClient(remoteClientName, remoteClientProductInstanceId, isAllowed)` | Creates an entry for this client |

## Properties

| Property | Description |
| -------- | ----------- |
| `RemoteClientName` | The UMP endpoint name the remote client announces |
| `RemoteClientProductInstanceId` | The product instance id the remote client announces |
| `IsAllowed` | True to let the client connect without asking again, and false to turn it away |

## Remarks

A remote client is recognized by its name and product instance id together, ignoring uppercase and lowercase differences. Neither one is enough on its own, so both are needed to recognize the same client again. An entry with both empty is skipped when the configuration is saved.
