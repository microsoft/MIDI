---
layout: sdk_reference_page
title: MidiRtpClientConnectConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to connect to a remote RTP-MIDI device
---

Describes a remote device for this PC to connect to. Pass it to `MidiRtpTransportManager.ConnectRtpClientAsync`. To connect again after the service restarts, also pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpClientConnectConfig()` | Creates a configuration with a new `ClientId`, `AutoReconnect` and `SendRecoveryJournal` turned on, and no `MatchCriteria` |

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client entry. Use it to reconnect and disconnect later, and to find the entry in `GetConfiguredClients()` |
| `Comment` | An optional comment saved with the entry in the configuration. The service doesn't use it |
| `Name` | What the remote device shows for this PC. Leave it empty to use this PC's name. At most 63 bytes in UTF-8 |
| `CustomEndpointName` | The name for the MIDI endpoint this connection creates. It's used from the moment the endpoint is created, so the endpoint and its MIDI 1.0 ports never appear under the remote device's own name first. Leave it empty to use the name the remote device sends. At most 255 characters |
| `MatchCriteria` | A [MidiRtpClientMatchCriteria]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientMatchCriteria/) that says which remote device to connect to. Required |
| `AutoReconnect` | When true, the service connects again whenever a try or a connection ends. When false, it connects once, and the entry is marked `Unavailable` when that try or connection ends |
| `SendRecoveryJournal` | When true, each packet carries a recovery journal, so the remote device can repair a lost Note Off. Only turn it off for a device that can't read one |

## Remarks

Calling `ConnectRtpClientAsync` with a `ClientId` that already exists replaces that entry's settings. It doesn't create a copy. Sending the same settings again doesn't make the service try again, so to retry an existing entry without changing it, use `ReconnectRtpClientAsync`.

See the [namespace overview]({{ site.baseurl }}/sdk-reference/Transports/Rtp/) for when the service tries again by itself.
