---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientPolicy
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Whether a remote Central connecting to this PC is let through without asking
---

Reported by `MidiBluetoothPeripheralStatus.ClientPolicy`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `RequireApproval` | `0` | A connecting device waits for someone to approve or deny it. |
| `AllowAny` | `1` | Any device which connects is given a MIDI endpoint straight away. |

## What "requiring approval" actually does

Bluetooth does not let Windows refuse the connection itself. A device which connects stays connected either way. What approval controls is whether it gets a MIDI endpoint and whether any data flows.

So under `RequireApproval`, an unapproved device sits there connected and silent until somebody decides. Poll `MidiBluetoothTransportManager.GetPendingPeripheralClients` to find those waiting, and resolve each with `ApprovePeripheralClientAsync` or `DenyPeripheralClientAsync`.

`AllowAny` is convenient on a machine you control, but it means any Bluetooth MIDI Central in range can send messages into this PC without being asked about.
