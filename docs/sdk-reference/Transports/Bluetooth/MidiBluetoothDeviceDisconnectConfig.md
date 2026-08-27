---
layout: sdk_reference_page
title: MidiBluetoothDeviceDisconnectConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Identifies a Bluetooth MIDI device to disconnect, and can be saved to stop it reconnecting
---

Implements `IMidiServiceTransportPluginConfig`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothDeviceDisconnectConfig(bluetoothDeviceId)` | Disconnects the device, and when saved leaves it listed but stops it connecting on its own. |
| `MidiBluetoothDeviceDisconnectConfig(bluetoothDeviceId, removeFromConfiguration)` | As above, except that saving with `removeFromConfiguration` set takes the entry out of the configuration file entirely. |

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The twelve hex digit Bluetooth address of the device. |
| `RemoveFromConfiguration` | When false, saving keeps the device listed but disabled. When true, saving removes the entry. |
| `TransportId` | The Bluetooth transport's GUID. |
| `ConfigJson` | The configuration file representation of this change. |

Disconnecting and forgetting are separate steps. Disconnecting alone lasts only until the service restarts, at which point a saved device connects again.
