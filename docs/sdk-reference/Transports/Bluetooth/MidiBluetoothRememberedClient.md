---
layout: sdk_reference_page
title: MidiBluetoothRememberedClient
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: A remembered allow or deny decision about a remote Central
---

An entry in the allow and deny lists reported by `MidiBluetoothPeripheralStatus.AllowedClients` and `DeniedClients`.

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothAddress` | The twelve hex digit address, which is what the decision is matched on. |
| `Name` | The name the device reported, carried so the configuration file is readable. It is not used for matching. |

Only a device whose address does not rotate can be remembered, so an entry here always has a usable address. That is also why `Name` is not the key: two devices can report the same name, and a device can change its name between connections.
