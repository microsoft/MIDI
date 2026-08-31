---
layout: sdk_reference_page
title: MidiBluetoothPeripheralStatus
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The state of this PC when published as a Bluetooth MIDI peripheral
---

Returned by `MidiBluetoothTransportManager.GetPeripheralStatus`, and carried on `MidiBluetoothPeripheralResponse`.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsRunning` | True when this PC is published as a Bluetooth MIDI device. |
| `Protocol` | The `MidiBluetoothProtocol` being advertised. Only one can be advertised at a time. |
| `AdvertisedName` | The name remote devices see. Windows takes this from the computer name and gives an application no way to override it, so it is reported rather than configured. |
| `SubscribedClientCount` | How many remote devices have subscribed. |
| `IsClientConnected` | True when a remote device is subscribed. This is the only sign that data can actually move. |
| `EndpointDeviceId` | The MIDI endpoint's device interface id. The endpoint represents the remote device, so it exists only while one is connected. |
| `EndpointDeviceInstanceId` | The endpoint's instance id, which is what an endpoint customization matches on. |
| `MessagesReceived` | Count of messages received from the connected device. |
| `MessagesSent` | Count of messages sent to the connected device. |
| `ConnectedClient` | The `MidiBluetoothPeripheralClient` which is connected, or null when nothing is. |

The MIDI endpoint here represents the remote device, in the same way a Network MIDI 2.0 host endpoint represents the remote client. There is nothing for an application to open until something connects.
