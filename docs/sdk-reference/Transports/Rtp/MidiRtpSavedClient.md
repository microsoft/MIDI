---
layout: sdk_reference_page
title: MidiRtpSavedClient
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: An RTP-MIDI client saved in the configuration file
---

An RTP-MIDI client saved in the configuration file. The service connects it every time it starts. `MidiRtpTransportManager.GetSavedClients` returns one of these for each saved client.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID that identifies this client. It's the same as `MidiRtpConfiguredClient.ClientId` when the client is set up in the service |
| `Comment` | The `Comment` saved with the entry, often the device's name. Empty when none was saved |
| `Name` | What the remote device shows for this PC. Empty means this PC's name |
| `CustomEndpointName` | The name the customer chose for the endpoint. Empty when the endpoint uses the name the remote device sends |
| `MatchCriteria` | A [MidiRtpClientMatchCriteria]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpClientMatchCriteria/) saying which remote device to connect to |
| `AutoReconnect` | False when the client connects once, and stays disconnected after the connection ends |
| `SendRecoveryJournal` | True when the client sends the recovery journal, so the remote device can repair a lost Note Off |
| `SendSpeedLimit` | How fast this PC sends to the remote device. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |
| `IsEnabled` | False when the service shouldn't connect the client |

## Remarks

This comes from the configuration file, not from the service. It tells you what the service connects the next time it starts, and it works even when the service isn't running.

`MatchCriteria` is a new copy each time you read it, so changing it changes nothing that's saved.

To forget a saved client, pass a `MidiRtpClientDisconnectConfig` to `MidiServiceTransportPluginConfigManager.SaveUpdate`.
