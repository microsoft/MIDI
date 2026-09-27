---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientDecisionResponse
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The result of approving or denying a remote device that connected to this PC
---

Returned by `MidiBluetoothTransportManager.ApprovePeripheralClientAsync`, `DenyPeripheralClientAsync` and `ForgetPeripheralClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True when the decision was applied |
| `ErrorCode` | A `MidiBluetoothPeripheralErrorCode` that says why it wasn't |
| `ErrorMessage` | The transport's own wording, which is usually more specific than the error code |
| `AppliedScope` | The `MidiBluetoothApprovalScope` that was really applied. It's not always the one you asked for |
| `BluetoothAddress` | The address the decision was applied to |
| `Name` | The name the remote device reported |
| `PersistRequired` | True when the decision has to be saved to the configuration to be kept after the service restarts |

## Checking what was applied

Read `AppliedScope` instead of assuming. A request to remember a device whose address changes is refused, not quietly changed to something else. So an app that assumes `Always` worked would tell people something that isn't true.

`PersistRequired` exists because the service applies a decision right away, but never saves the configuration itself. When it's true, save the current lists with `MidiBluetoothPeripheralClientListConfig`, or the decision is lost the next time the service restarts.
