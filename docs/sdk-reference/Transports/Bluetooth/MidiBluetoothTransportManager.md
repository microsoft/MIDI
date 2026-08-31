---
layout: sdk_reference_page
title: MidiBluetoothTransportManager
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The primary class used to discover and connect Bluetooth MIDI devices, and to publish this PC as a Bluetooth MIDI peripheral
---

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if this transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `GetAvailableDevices()` | Returns every Bluetooth MIDI device this PC has seen advertising, as a collection of `MidiBluetoothDeviceInformation`, whether or not each is connected. |
| `GetDevice(bluetoothDeviceId)` | Returns the `MidiBluetoothDeviceInformation` for one device, or null when no device with that address has been discovered. |
| `ConnectDeviceAsync(connectConfig)` | Connects to a device and creates a MIDI endpoint for it. Returns a `MidiBluetoothDeviceConnectResponse`. |
| `DisconnectDeviceAsync(disconnectConfig)` | Disconnects a device and removes its MIDI endpoint. Returns a `MidiBluetoothDeviceDisconnectResponse`. |
| `StartPeripheralAsync(peripheralConfig)` | Publishes this PC so other devices can connect to it. Returns a `MidiBluetoothPeripheralResponse`. |
| `StopPeripheralAsync()` | Stops publishing this PC. Returns a `MidiBluetoothPeripheralResponse`. |
| `GetPeripheralStatus()` | Returns the current `MidiBluetoothPeripheralStatus`. |

Connecting and disconnecting are asynchronous because the radio has to find the device, open its GATT service, and subscribe to a characteristic. That takes seconds, and longer for a device which is asleep.

## Connecting and remembering are separate steps

Passing a `MidiBluetoothDeviceConnectConfig` to `ConnectDeviceAsync` connects the device now, for this session only. Passing the same object to `MidiServiceTransportPluginConfigManager.SaveUpdate` writes it to the configuration file so it reconnects after the service restarts.

```cpp
auto config = MidiBluetoothDeviceConnectConfig(L"48B6201A719D");

auto response = co_await MidiBluetoothTransportManager::ConnectDeviceAsync(config);

if (response.Success())
{
    // keep it across service restarts
    MidiServiceTransportPluginConfigManager::SaveUpdate(config);
}
```

The same applies in reverse. Saving a `MidiBluetoothDeviceDisconnectConfig` stops the device connecting on its own; constructing it with `removeFromConfiguration` set takes the entry out of the file entirely.

## Devices which are not present

A connection request is remembered rather than performed, so a request for a device which is switched off succeeds and connects later, when the device appears. `MidiBluetoothDeviceConnectResponse.IsKnown` and `MidiBluetoothDeviceInformation.IsPresent` are what distinguish "connecting now" from "waiting for it to appear", which otherwise look identical.
