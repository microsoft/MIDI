---
layout: sdk_reference_page
title: MidiNetworkRemoteClientForgetResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of dropping a remembered decision for one remote client
---

Returned by `MidiNetworkTransportManager.ForgetRemoteClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the request was about |
| `RemoteClientName` | The UMP Endpoint Name of the remote client the request was about |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client the request was about |
| `Success` | True if the host no longer has a decision for that remote client |
| `ErrorCode` | A `MidiNetworkRemoteClientForgetErrorCode` when `Success` is false |
| `ErrorMessage` | An error message people can read |

## Remarks

Forgetting a client the host has no decision for reports success, because what you asked for is already true.

If `Success` is false and the error is `UnrecognizedCommand`, the service is older than this command. You can still save the lists, but the old decision stays until the service restarts.
