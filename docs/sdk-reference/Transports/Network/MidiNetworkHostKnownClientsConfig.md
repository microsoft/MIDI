---
layout: sdk_reference_page
title: MidiNetworkHostKnownClientsConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The allow and deny decisions saved for a Network MIDI 2.0 host
---

Pass to `MidiServiceTransportPluginConfigManager.SaveUpdate` to make a host's allow and deny decisions outlive a service restart.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostKnownClientsConfig()` | Create an empty config |
| `MidiNetworkHostKnownClientsConfig(hostId)` | Create a config for the specified host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry these decisions belong to |
| `KnownClients` | The complete set of [MidiNetworkKnownRemoteClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkKnownRemoteClient/) the host has been told about |

## Remarks

This is a saved record, not a command. Telling the service about a single decision is done with `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`, which acts on it immediately. The service remembers a decision for as long as it is running but never writes the configuration file, so saving one of these is what makes a decision survive a restart.

`KnownClients` must hold the complete set for the host, not only what changed. Both saved lists are replaced by what it holds, so read the current set first, change it, and save the whole thing. Leaving a client out is how a decision is withdrawn, which puts the client back to being one the host has never been told about.

Saving an empty `KnownClients` clears both lists for the host.
