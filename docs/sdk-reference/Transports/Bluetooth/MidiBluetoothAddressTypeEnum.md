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
| `Public` | `1` | A public address, which doesn't change |
| `Random` | `2` | A random address, which the device changes every so often for privacy |

You can't use a random address to recognize a phone or tablet the next time it connects to this PC. Pairing is what gives a remote device an identity that doesn't change.
