---
layout: sdk_reference_page
title: MidiBluetoothAddressType
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Whether a Bluetooth address is public or random
---

Reported on `MidiBluetoothPeripheralClient`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Unknown` | `0` | Not reported |
| `Public` | `1` | A public address, which is stable |
| `Random` | `2` | A random address, re-generated periodically for privacy |

A random address is not a durable way to recognize a phone or tablet which has connected to this PC. Pairing is what gives a remote device a stable identity.
