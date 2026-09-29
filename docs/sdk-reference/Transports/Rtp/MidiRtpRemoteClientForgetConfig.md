---
layout: sdk_reference_page
title: MidiRtpRemoteClientForgetConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to drop a remembered allow or deny decision for a remote RTP-MIDI device
---

Pass to `MidiRtpTransportManager.ForgetRemoteClientAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpRemoteClientForgetConfig()` | Creates an empty configuration |
| `MidiRtpRemoteClientForgetConfig(hostId, remoteClientName)` | Creates a configuration with both values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host on this PC |
| `RemoteClientName` | The name of the remote device, as the host's decision has it |

## Remarks

Forgetting doesn't turn anything away. A connection that's already up keeps running. Only the decision is dropped, so the next time that remote device asks, it's judged by the host's `RemoteClientPolicy` alone.

This changes the running service right away. The saved allow and deny lists are separate, and you save them with `MidiRtpHostKnownClientsConfig`. An app that forgets a decision should do both, or the decision comes back the next time the service starts.
