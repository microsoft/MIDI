---
layout: sdk_reference_page
title: MidiBluetoothRememberedClient
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: A remembered allow or deny decision about a remote device
---

An entry in the allow and deny lists reported by `MidiBluetoothPeripheralStatus.AllowedClients` and `DeniedClients`.

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothAddress` | The address as twelve hex digits, which is what the decision is matched on |
| `Name` | The name the device reported, kept so a person can recognize the entry. It isn't used for matching |

Only a device whose address doesn't change can be remembered, so an entry here always has an address you can use. That's also why `Name` isn't the key. Two devices can report the same name, and a device can change its name between connections.
