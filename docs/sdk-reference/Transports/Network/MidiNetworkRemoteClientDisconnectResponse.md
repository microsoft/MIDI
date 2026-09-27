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
| `HostId` | The GUID of the host the request was about |
| `RemoteClientName` | The UMP Endpoint Name of the remote client the request was about |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client the request was about |
| `Success` | True if the client's session was ended |
| `ErrorCode` | A `MidiNetworkRemoteClientDisconnectErrorCode` when `Success` is false |
| `ErrorMessage` | An error message people can read |

## Remarks

A successful disconnect doesn't stop that remote client from asking to connect again later.
