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
| `HostId` | Host GUID targeted by the request |
| `RemoteClientName` | Remote client UMP Endpoint Name targeted by the request |
| `RemoteClientProductInstanceId` | Remote client Product Instance Id targeted by the request |
| `Success` | True if the host no longer holds a decision for that remote client |
| `ErrorCode` | `MidiNetworkRemoteClientForgetErrorCode` when `Success` is false |
| `ErrorMessage` | Human-readable error text |

## Remarks

Forgetting an identity the host holds no decision for reports success, because what the caller asked for is already true.

`Success` being false with `UnrecognizedCommand` means the service predates this command. The saved lists can still be rewritten, but the old decision stays in force until the service restarts.
