---
layout: sdk_reference_page
title: MidiRtpClientConnectResponse
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: Result of a request to connect an RTP-MIDI client
---

Returned by `MidiRtpTransportManager.ConnectRtpClientAsync` and `ReconnectRtpClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `ClientId` | The GUID of the client entry the request was about |
| `Success` | True if the service took the request |
| `ErrorCode` | A `MidiRtpClientConnectErrorCode` when `Success` is false |
| `ErrorMessage` | A description of the failure that people can read |

## Remarks

Check `Success` first. When it's false, `ErrorCode` tells your code why, and `ErrorMessage` is a description you can show to people.

`Success` means the service has the entry, not that the remote device answered. Connecting happens in the background. To follow it, read `EntryState` on the entry from `GetConfiguredClients()`, or wait for its endpoint to appear.
