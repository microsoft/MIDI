---
layout: sdk_reference_page
title: MidiBluetoothDeviceConnectConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Identifies a Bluetooth MIDI device to connect to, and can be saved to make the connection persist
---

Implements `IMidiServiceTransportPluginConfig`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothDeviceConnectConfig(bluetoothDeviceId)` | Creates a configuration for the device with this Bluetooth address, as twelve hex digits |

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The device's Bluetooth address, as twelve hex digits |
| `Comment` | An optional comment saved with the entry in the configuration. It doesn't change the connection. It's there to make the entry easier for a person to recognize |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | This connection, as JSON |

Pass this to `MidiBluetoothTransportManager.ConnectDeviceAsync` to connect now. Pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate` to have the device connect again after the service restarts. They're separate steps, so a connection the service rejects is never saved.
