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
| `MidiBluetoothDeviceConnectConfig(bluetoothDeviceId)` | Creates a configuration for the device at the given twelve hex digit Bluetooth address. |

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The twelve hex digit Bluetooth address of the device. |
| `Comment` | An optional comment written into the configuration file alongside the entry. It has no effect on the connection and exists to make the file readable. |
| `TransportId` | The Bluetooth transport's GUID. |
| `ConfigJson` | The configuration file representation of this connection. |

Pass this to `MidiBluetoothTransportManager.ConnectDeviceAsync` to connect now, and to `MidiServiceTransportPluginConfigManager.SaveUpdate` to have the device reconnect after the service restarts. The two are separate steps, so a connection which the service rejects is never written to the configuration file.
