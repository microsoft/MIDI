---
layout: sdk_reference_page
title: MidiNetworkRemoteClientDisconnectResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of disconnecting one remote client from a host on this PC
---

Returned by `MidiNetworkTransportManager.DisconnectRemoteClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | Host GUID targeted by the request |
| `RemoteClientName` | Remote client UMP Endpoint Name targeted by the request |
| `RemoteClientProductInstanceId` | Remote client Product Instance Id targeted by the request |
| `Success` | True if the client session was disconnected |
| `ErrorCode` | `MidiNetworkRemoteClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | Human-readable error text |

## Remarks

A successful disconnect does not deny future reconnect requests from that remote client.
