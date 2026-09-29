---
layout: sdk_reference_page
title: MidiRtpHostKnownClientsConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: The allow and deny decisions saved for an RTP-MIDI host
---

Pass this to `MidiServiceTransportPluginConfigManager.SaveUpdate`, so a host's allow and deny decisions are kept after the service restarts.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpHostKnownClientsConfig()` | Creates an empty configuration |
| `MidiRtpHostKnownClientsConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host entry these decisions belong to |
| `KnownClients` | Every [MidiRtpKnownRemoteClient]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpKnownRemoteClient/) the host has a decision for |

## Remarks

This is a saved record, not a command, so there's nothing to send. The running service already has each decision from `ApproveOrDenyRemoteClientConnectRequestAsync`. It never saves them itself, and saving one of these is what keeps a decision after a restart.

`KnownClients` must hold every remote device for the host, not just the ones that changed. Both saved lists are replaced with what it holds. So start from `MidiRtpConfiguredHost.KnownRemoteClients`, change it, and save the whole thing. Leaving a remote device out is how you take back a decision.

Leaving a remote device out only changes what's read the next time the service starts. To take the decision back in the running service too, call `ForgetRemoteClientAsync`. Otherwise, the old decision stays until the service restarts.

Saving an empty `KnownClients` clears both lists for the host.
