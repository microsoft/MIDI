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
| `MidiBluetoothPeripheralConfig()` | Creates a configuration that advertises Bluetooth Low Energy MIDI 1.0 |
| `MidiBluetoothPeripheralConfig(protocol)` | Creates a configuration that advertises this `MidiBluetoothProtocol` |

## Properties

| Property | Description |
| -------- | ----------- |
| `Protocol` | Which `MidiBluetoothProtocol` to advertise. Only one can be advertised at a time, so a MIDI 1.0 device can't connect while MIDI 2.0 is selected |
| `IsEnabled` | Save with this set to false to stop publishing this PC when the service next starts. Otherwise, stopping the peripheral only lasts until the service restarts |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | This setting, as JSON |

You can't set the advertised name. Windows uses the computer name, and it gives apps no way to change it. So the name is reported in `MidiBluetoothPeripheralStatus.AdvertisedName` instead.
