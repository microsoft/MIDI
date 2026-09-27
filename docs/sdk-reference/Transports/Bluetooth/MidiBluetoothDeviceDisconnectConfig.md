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
| `MidiBluetoothDeviceDisconnectConfig(bluetoothDeviceId)` | Disconnects the device. If you save it, the device stays listed but stops connecting on its own |
| `MidiBluetoothDeviceDisconnectConfig(bluetoothDeviceId, removeFromConfiguration)` | The same, except that if `removeFromConfiguration` is true, saving removes the entry from the configuration completely |

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The device's Bluetooth address, as twelve hex digits |
| `RemoveFromConfiguration` | When false, saving keeps the device listed but turned off. When true, saving removes the entry |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | This change, as JSON |

Disconnecting and forgetting are separate steps. Disconnecting alone lasts only until the service restarts. Then a saved device connects again.
