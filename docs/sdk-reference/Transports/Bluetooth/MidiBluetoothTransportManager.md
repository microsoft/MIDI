---
layout: sdk_reference_page
title: MidiBluetoothTransportManager
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The primary class used to discover and connect Bluetooth MIDI devices, and to publish this PC as a Bluetooth MIDI peripheral
---

Finds and connects Bluetooth MIDI devices, and publishes this PC as a Bluetooth MIDI peripheral.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | True if this transport is available in the service |
| `TransportId` | The GUID of this transport |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `GetAvailableDevices()` | Returns a `MidiBluetoothDeviceInformation` for every Bluetooth MIDI device this PC has seen advertising, whether or not it's connected |
| `GetDevice(bluetoothDeviceId)` | Returns the `MidiBluetoothDeviceInformation` for one device, or null if no device with that address has been found |
| `ConnectDeviceAsync(connectConfig)` | Connects to a device and creates a MIDI endpoint for it. Returns a `MidiBluetoothDeviceConnectResponse` |
| `DisconnectDeviceAsync(disconnectConfig)` | Disconnects a device and removes its MIDI endpoint. Returns a `MidiBluetoothDeviceDisconnectResponse` |
| `StartPeripheralAsync(peripheralConfig)` | Publishes this PC so other devices can connect to it. Returns a `MidiBluetoothPeripheralResponse` |
| `StopPeripheralAsync()` | Stops publishing this PC. Returns a `MidiBluetoothPeripheralResponse` |
| `GetPeripheralStatus()` | Returns the current `MidiBluetoothPeripheralStatus` |
| `GetPendingPeripheralClients()` | Returns the remote devices that have connected and are waiting for a decision, as `MidiBluetoothPeripheralClient` entries |
| `ApprovePeripheralClientAsync(bluetoothAddress, scope)` | Approves a waiting remote device for this `MidiBluetoothApprovalScope`. Returns a `MidiBluetoothPeripheralClientDecisionResponse` |
| `DenyPeripheralClientAsync(bluetoothAddress, scope)` | Denies a waiting remote device. Returns a `MidiBluetoothPeripheralClientDecisionResponse` |
| `ForgetPeripheralClientAsync(bluetoothAddress)` | Forgets a remembered allow or deny, so the device is asked about again next time. Returns a `MidiBluetoothPeripheralClientDecisionResponse` |
| `GetRadioInformation()` | Returns what this PC's Bluetooth radio can do, as `MidiBluetoothRadioInformation`, or null with a service that's too old to report it |
| `GetDefaultOfflineRetentionSeconds()` | Returns the transport-wide value that every device set to `UseTransportDefault` uses. It's never `UseTransportDefault` itself |

One manager covers both Bluetooth Low Energy MIDI 1.0 and 2.0. The transport picks which protocol to use with each device, and prefers MIDI 2.0 whenever a device offers it.

Connecting and disconnecting are asynchronous because the radio has to find the device, open its GATT service, and subscribe to a characteristic. That takes a few seconds, and longer for a device that's asleep.

## Approving a remote device

Bluetooth doesn't let Windows refuse an incoming connection. A remote device that connects while the policy is `RequireApproval` stays connected, but it has no MIDI endpoint, and no data moves until someone decides. So if your app never calls `GetPendingPeripheralClients`, the device just sits there, stuck and silent.

The address you pass to `ApprovePeripheralClientAsync` and `DenyPeripheralClientAsync` must be the one reported for the device that's waiting. A decision made from an out-of-date list is rejected with `ClientIdentityMismatch`, instead of being applied to whoever connected since.

## Connecting and remembering are separate steps

Passing a `MidiBluetoothDeviceConnectConfig` to `ConnectDeviceAsync` connects the device now, until the service restarts. Passing the same object to `MidiServiceTransportPluginConfigManager.SaveUpdate` saves it to the configuration, so it connects again after the service restarts.

```cpp
auto config = MidiBluetoothDeviceConnectConfig(L"48B6201A719D");

auto response = co_await MidiBluetoothTransportManager::ConnectDeviceAsync(config);

if (response.Success())
{
    // keep it across service restarts
    MidiServiceTransportPluginConfigManager::SaveUpdate(config);
}
```

It works the other way too. Saving a `MidiBluetoothDeviceDisconnectConfig` stops the device from connecting on its own. If you create it with `removeFromConfiguration` set to true, saving removes the entry completely.

## Devices that aren't present

A connection request is saved and carried out when it can be. So a request for a device that's turned off succeeds, and the device connects later, when it shows up. `MidiBluetoothDeviceConnectResponse.IsKnown` and `MidiBluetoothDeviceInformation.IsPresent` tell "connecting now" apart from "waiting for it to show up," which otherwise look the same.
