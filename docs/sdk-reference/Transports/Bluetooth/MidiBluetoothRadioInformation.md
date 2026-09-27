---
layout: sdk_reference_page
title: MidiBluetoothRadioInformation
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: What the Bluetooth radio on this PC can actually do
---

Returned by `MidiBluetoothTransportManager.GetRadioInformation`.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsPresent` | True when this PC has a Bluetooth radio at all |
| `IsLowEnergySupported` | True when that radio supports Bluetooth Low Energy, which is what Bluetooth MIDI uses |
| `IsCentralRoleSupported` | Needed to connect to a device. Without it, nothing can be found or connected |
| `IsPeripheralRoleSupported` | Needed to publish this PC so other devices can connect to it |

## Why your app should check this

On a PC with no Bluetooth, or with a radio that can't advertise, the transport still loads. Every call still works, but nothing happens. Without checking here, your app can't explain to people why no devices ever show up.

`IsPeripheralRoleSupported` is the one that's most often false. Plenty of radios support the central role but not the peripheral role. That isn't a problem with the transport. Connecting to devices still works normally, and only publishing this PC isn't available. Check it before you offer that choice, instead of letting `StartPeripheralAsync` fail.

`GetRadioInformation` returns null with a service that's too old to report this. Treat that differently from "no radio." Telling people the PC has no Bluetooth when the service just didn't say would be worse than saying nothing.
