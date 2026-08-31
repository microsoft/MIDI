---
layout: sdk_reference_page
title: MidiBluetoothPeripheralConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Configures publishing this PC as a Bluetooth MIDI peripheral
---

Implements `IMidiServiceTransportPluginConfig`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothPeripheralConfig()` | Creates a configuration which advertises Bluetooth Low Energy MIDI 1.0. |
| `MidiBluetoothPeripheralConfig(protocol)` | Creates a configuration which advertises the given `MidiBluetoothProtocol`. |

## Properties

| Property | Description |
| -------- | ----------- |
| `Protocol` | Which `MidiBluetoothProtocol` to advertise. Only one can be advertised at a time, so a MIDI 1.0 device cannot connect while MIDI 2.0 is selected. |
| `IsEnabled` | Saving with this false is how a caller stops the peripheral being published on the next service start, since stopping it is otherwise only for this session. |
| `TransportId` | The Bluetooth transport's GUID. |
| `ConfigJson` | The configuration file representation of this setting. |

The advertised name is not configurable. Windows takes it from the computer name and the GATT service provider gives an application no way to override it, so it is reported through `MidiBluetoothPeripheralStatus.AdvertisedName` instead.
