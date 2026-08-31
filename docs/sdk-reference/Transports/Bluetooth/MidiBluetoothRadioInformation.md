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
| `IsPresent` | True when this PC has a Bluetooth radio at all. |
| `IsLowEnergySupported` | True when that radio supports Bluetooth Low Energy, which is what Bluetooth MIDI uses. |
| `IsCentralRoleSupported` | Required to connect out to a device. Without it, nothing can be discovered or connected. |
| `IsPeripheralRoleSupported` | Required to publish this PC so other devices can connect to it. |

## Why an application should check this

A machine with no Bluetooth, or with a radio which cannot advertise, still loads the transport successfully. Every call keeps working and simply achieves nothing, so without checking here an application has no way to explain to the customer why no devices ever appear.

`IsPeripheralRoleSupported` is the one most often false. Plenty of radios support the Central role but not the Peripheral role, and that is not a failure of the transport: connecting out to devices keeps working normally, and only publishing this PC is unavailable. Check it before offering that option rather than letting `StartPeripheralAsync` fail.

Returns `null` on a service too old to report this, which is worth distinguishing from "no radio" — saying the PC has no Bluetooth when the service simply did not say would be worse than saying nothing.
