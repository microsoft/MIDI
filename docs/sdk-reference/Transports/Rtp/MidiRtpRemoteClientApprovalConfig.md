---
layout: sdk_reference_page
title: MidiRtpRemoteClientApprovalConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to approve or deny a remote RTP-MIDI device
---

Pass to `MidiRtpTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpRemoteClientApprovalConfig()` | Creates an empty configuration |
| `MidiRtpRemoteClientApprovalConfig(hostId, remoteClientName, approve, restrictScopeToThisRequestOnly)` | Creates a configuration with all of these values set |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the remote device asked to connect to |
| `RemoteClientName` | The remote device's name, exactly as `MidiRtpPendingRemoteClient.RemoteClientName` reports it |
| `Approve` | True to let the remote device in, and false to turn it away |
| `ScopeIsThisRequestOnly` | True to decide about the request that's waiting now, and nothing more. False to remember the decision for later requests from the same remote device |

## Remarks

When `ScopeIsThisRequestOnly` is true, the remote device has to be waiting, or the request fails with `PendingRemoteClientNotFound`. Denying this way still turns down the remote device's next request, so it stops asking.

When `ScopeIsThisRequestOnly` is false, you can decide about a remote device before it ever asks. The service remembers the decision until it restarts. To keep it after that, also save a [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/) for the host. A host remembers up to 256 decisions.

Denying a remote device also ends any connection it already has to the host.

`RemoteClientName` isn't trimmed or changed in any way, because it has to match the name the remote device sends. Uppercase and lowercase differences don't matter.
