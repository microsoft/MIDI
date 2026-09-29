---
layout: sdk_reference_page
title: MidiBluetoothSavedDevice
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: A Bluetooth MIDI device saved in the configuration file
---

A Bluetooth MIDI device saved in the configuration file. `MidiBluetoothTransportManager.GetSavedDevices` returns one of these for each saved device.

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The device's Bluetooth address as twelve hex digits, the same form `MidiBluetoothDeviceInformation` uses |
| `Comment` | The `Comment` from the `MidiBluetoothDeviceConnectConfig` that saved the device, which is usually its name. Empty when none was saved |
| `IsEnabled` | False when the device was kept but switched off by saving a `MidiBluetoothDeviceDisconnectConfig`. The service doesn't connect it when it starts |
| `OfflineRetentionSeconds` | The offline retention saved for this device, as seconds or a named `MidiBluetoothOfflineRetention` value. `UseTransportDefault` when none is saved |

## Saved isn't the same as connected

This list comes from what's saved, not from what's happening right now. It tells you what the service will do the next time it starts, and it works even when the service isn't running.

A device can be saved and not connected, because it's switched off or out of range. A device can also be connected and not saved, because an app connected it without saving it. To show both, combine this list with `MidiBluetoothTransportManager.GetAvailableDevices`, matching on `BluetoothDeviceId`.

A saved device that this PC hasn't found yet isn't in `GetAvailableDevices`. This list is how your app can still show it, using `Comment` as its name.

A device that only has a setting saved, such as its offline retention, isn't in this list. Saving a setting doesn't ask the service to connect the device.

"Saved" and "configured" mean different things across the transports. Saved is what's in the configuration file. Configured, as in `GetConfiguredHosts` for the network transports, is what the running service holds right now.
