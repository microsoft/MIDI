---
layout: sdk_reference_page
title: MidiBluetoothApprovalScope
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How long a decision about a remote device applies
---

Passed to `MidiBluetoothTransportManager.ApprovePeripheralClientAsync` and `DenyPeripheralClientAsync`, and reported back as `AppliedScope` on the `MidiBluetoothPeripheralClientDecisionResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Once` | `0` | Applies only to the connection that's waiting right now. If the same device connects again, you're asked again |
| `UntilRestart` | `1` | Kept in memory, so it's forgotten when the service restarts |
| `Always` | `2` | Applied right away, but the service doesn't save it. It's kept after a restart only if you also save it with `MidiBluetoothPeripheralClientListConfig` |

`Always` only works for a device whose Bluetooth address doesn't change. Phones and tablets usually change their address for privacy, so there's nothing that stays the same to remember them by. Check `MidiBluetoothPeripheralClient.IsRememberable` before you offer this choice. Asking for `Always` for a changing address is refused, not quietly changed to something else. The response reports the failure, and `AppliedScope` tells you what was really applied.
