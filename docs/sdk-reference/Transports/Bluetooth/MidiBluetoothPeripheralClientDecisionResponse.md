---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientDecisionResponse
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The result of approving or denying a remote Central which connected to this PC
---

Returned by `MidiBluetoothTransportManager.ApprovePeripheralClientAsync`, `DenyPeripheralClientAsync` and `ForgetPeripheralClientAsync`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Success` | True when the decision was applied. |
| `ErrorCode` | A `MidiBluetoothPeripheralErrorCode` describing why it was not. |
| `ErrorMessage` | The transport's own wording, which is usually more specific than the error code. |
| `AppliedScope` | The `MidiBluetoothApprovalScope` actually applied, which is not always the one asked for. |
| `BluetoothAddress` | The address the decision was applied to. |
| `Name` | The name the remote device reported. |
| `PersistRequired` | True when the decision has to be written to the configuration file to survive a service restart. |

## Checking what was actually applied

`AppliedScope` is worth reading rather than assuming. A request to remember a device whose address rotates is refused rather than quietly downgraded, so a caller which assumes `Always` was honored would tell the customer something untrue.

`PersistRequired` exists because the service applies a decision immediately but never writes the configuration file itself. When it is true, save the current lists with `MidiBluetoothPeripheralClientListConfig` or the decision is lost on the next service restart.
