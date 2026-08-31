---
layout: sdk_reference_page
title: MidiBluetoothDeviceInformation
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Information about a Bluetooth MIDI device this PC has discovered
---

Returned by `MidiBluetoothTransportManager.GetAvailableDevices` and `MidiBluetoothTransportManager.GetDevice`.

## Properties

| Property | Description |
| -------- | ----------- |
| `BluetoothDeviceId` | The twelve hex digit Bluetooth address, and the key for every operation in this namespace. This is not a Windows device interface id. |
| `BluetoothAddress` | The same address as a number, for lining up with the `Windows.Devices.Bluetooth` APIs. |
| `Name` | The name the device reports. Empty until the device has been heard from for long enough to resolve it. |
| `SelectedProtocol` | The `MidiBluetoothProtocol` in use. Unknown until the device is connected, because reading it means reading the device's characteristics. |
| `IsConnected` | True when the device is connected to this PC. |
| `IsPaired` | True when the device is paired with this PC. Pairing is not required for Bluetooth MIDI. |
| `IsPresent` | True while the device is advertising. Bluetooth MIDI peripherals sleep aggressively, so a device which is not present is usually asleep rather than gone. |
| `SignalStrengthDecibelMilliwatts` | The signal strength of the most recent advertisement. A connected device has stopped advertising, so this stops being meaningful. |
| `LastSeenAgo` | How long ago the device was last heard from. |
| `HasEndpoint` | True when a MIDI endpoint exists for this device. |
| `EndpointDeviceId` | The MIDI endpoint's device interface id, when there is one. |
| `EndpointDeviceInstanceId` | The endpoint's instance id, which is what an endpoint customization matches on. |
| `MessagesReceived` | Count of messages received from the device. |
| `MessagesSent` | Count of messages sent to the device. |
| `ConnectionInterval` | What the link actually negotiated. Zero when not connected. |
| `LastConnectError` | The transport's own wording for the most recent connection failure. This is more specific than the error code can be. |
| `LastConnectErrorHResult` | The HRESULT behind that failure. |
| `LastSendErrorHResult` | The HRESULT from the most recent failed send. |

The endpoint's native data format is not reported here. Read it from the endpoint itself through `MidiEndpointDeviceInformation` and `Windows.Devices.Midi2.Enumeration.MidiEndpointNativeDataFormat`.
