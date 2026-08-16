---
layout: sdk_reference_page
title: MidiNetworkRemoteClientApprovalResponse
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: Result of approving or denying a waiting remote client
---

Returned by `MidiNetworkTransportManager.ApproveOrDenyRemoteClientConnectRequestAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID of the host the decision applied to |
| `ClientId` | The GUID of the resulting client entry, when one was created |
| `RemoteClientName` | The UMP Endpoint Name of the remote client |
| `RemoteClientProductInstanceId` | The Product Instance Id of the remote client |
| `Success` | True if the decision was applied |
| `ErrorCode` | A `MidiNetworkRemoteClientApprovalErrorCode` when `Success` is false |
| `ErrorMessage` | A human-readable description of the failure |

## Remarks

A decision naming a client the service has never seen returns `PendingRemoteClientNotFound`. This is normal if the client gave up while the user was deciding.