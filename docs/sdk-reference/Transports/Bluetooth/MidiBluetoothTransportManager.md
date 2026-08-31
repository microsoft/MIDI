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
| `GetPendingPeripheralClients()` | Returns the remote Centrals which have connected and are waiting for a decision, as `MidiBluetoothPeripheralClient` entries. |
| `ApprovePeripheralClientAsync(bluetoothAddress, scope)` | Approves a waiting remote Central for the given `MidiBluetoothApprovalScope`. Returns a `MidiBluetoothPeripheralClientDecisionResponse`. |
| `DenyPeripheralClientAsync(bluetoothAddress, scope)` | Denies a waiting remote Central. |
| `ForgetPeripheralClientAsync(bluetoothAddress)` | Drops a remembered allow or deny, so the device is asked about again next time. |
| `GetRadioInformation()` | Returns what this PC's Bluetooth radio can do, as `MidiBluetoothRadioInformation`, or null on a service too old to report it. |
| `GetDefaultOfflineRetentionSeconds()` | Returns the transport-wide value every device set to `UseTransportDefault` falls back to. Never `UseTransportDefault` itself. |

One manager covers both Bluetooth Low Energy MIDI 1.0 and 2.0. Which protocol a device speaks is chosen by the transport, which prefers MIDI 2.0 whenever a device offers it.

## Approving a remote Central

Bluetooth does not let Windows refuse an incoming connection. A remote Central which connects while the policy is `RequireApproval` stays connected but has no MIDI endpoint and moves no data until it is decided about, so an application which never calls `GetPendingPeripheralClients` leaves it stuck there silently.

The address passed to `ApprovePeripheralClientAsync` and `DenyPeripheralClientAsync` must be the one reported for the client which is waiting. A decision made against a stale list is rejected with `ClientIdentityMismatch` rather than applied to whoever connected since.

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
