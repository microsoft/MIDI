---
layout: sdk_reference_page
title: MidiBluetoothApprovalScope
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How long a decision about a remote Central applies
---

Passed to `MidiBluetoothTransportManager.ApprovePeripheralClientAsync` and `DenyPeripheralClientAsync`, and reported back as `AppliedScope` on the `MidiBluetoothPeripheralClientDecisionResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Once` | `0` | Applies to the connection waiting right now and nothing beyond it. The same device connecting again asks again. |
| `UntilRestart` | `1` | Held in memory, so a service restart forgets it. |
| `Always` | `2` | Applied immediately, but the service does not write it down. It survives a restart only if you also save it with `MidiBluetoothPeripheralClientListConfig`. |

`Always` is only accepted for a device whose Bluetooth address does not rotate. Phones and tablets normally rotate their address for privacy, so there is nothing stable to remember them by. Check `MidiBluetoothPeripheralClient.IsRememberable` before offering the option, because asking for `Always` on a rotating address is refused rather than quietly downgraded: the response reports the failure and `AppliedScope` tells you what was actually applied.
