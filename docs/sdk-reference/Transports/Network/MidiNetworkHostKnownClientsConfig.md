---
layout: sdk_reference_page
title: MidiNetworkHostKnownClientsConfig
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The allow and deny decisions saved for a Network MIDI 2.0 host
---

Pass this to `MidiServiceTransportPluginConfigManager.SaveUpdate` so a host's allow and deny decisions are kept after the service restarts.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkHostKnownClientsConfig()` | Creates an empty configuration |
| `MidiNetworkHostKnownClientsConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry these decisions belong to |
| `KnownClients` | Every [MidiNetworkKnownRemoteClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkKnownRemoteClient/) the host has a decision for |

## Remarks

This is a saved record, not a command. To tell the service about one decision, use `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`, which acts on it right away. The service remembers a decision while it's running, but never saves the configuration. Saving one of these is what keeps a decision after a restart.

`KnownClients` must hold every client for the host, not just the ones that changed. Both saved lists are replaced with what it holds. So read the current list first, change it, and save the whole thing. Leaving a client out is how you take back a decision. The client goes back to being one the host has never been told about.

Leaving a client out only changes what's read the next time the service starts. The running service keeps its own copy of the lists, so also take the decision back there with [ForgetRemoteClientAsync]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkRemoteClientForgetConfig/). Otherwise, the old decision stays until the service restarts.

Saving an empty `KnownClients` clears both lists for the host.
