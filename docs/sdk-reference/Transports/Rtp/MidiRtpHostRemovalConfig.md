---
layout: sdk_reference_page
title: MidiRtpHostRemovalConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to remove an RTP-MIDI host
---

Pass it to `MidiRtpTransportManager.RemoveRtpHostAsync` to remove a host from the running service, and to `MidiServiceTransportPluginConfigManager.SaveUpdate` to take it out of the configuration.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpHostRemovalConfig()` | Creates an empty configuration |
| `MidiRtpHostRemovalConfig(hostId)` | Creates a configuration for this host |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host to remove |

## Remarks

Removing a host ends every connection to it, stops advertising it, and frees its ports. Saving the removal also takes the host's saved allow and deny decisions, and its devices' own sending speeds, out of the configuration.

`RemoveRtpHostAsync` only changes the running service, and `SaveUpdate` only changes the configuration. To remove a host for good, do both.
