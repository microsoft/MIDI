---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientPolicy
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Whether a remote device connecting to this PC is let in without asking
---

Reported by `MidiBluetoothPeripheralStatus.ClientPolicy`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `RequireApproval` | `0` | A device that connects waits for someone to approve or deny it |
| `AllowAny` | `1` | Any device that connects gets a MIDI endpoint right away |

## What approval does

Bluetooth doesn't let Windows refuse the connection itself. A device that connects stays connected either way. Approval controls whether it gets a MIDI endpoint, and whether any data gets through.

So with `RequireApproval`, a device that hasn't been approved stays connected and silent until someone decides. Call `MidiBluetoothTransportManager.GetPendingPeripheralClients` from time to time to find the ones waiting, and decide on each with `ApprovePeripheralClientAsync` or `DenyPeripheralClientAsync`.

`AllowAny` is handy on a PC you control. But it means any Bluetooth MIDI device in range can connect and send messages into this PC without anyone being asked.
