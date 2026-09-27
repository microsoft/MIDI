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
| `IsRunning` | True when this PC is published as a Bluetooth MIDI device |
| `Protocol` | The `MidiBluetoothProtocol` being advertised. Only one can be advertised at a time |
| `AdvertisedName` | The name remote devices see. Windows takes it from the computer name and gives apps no way to change it, so it's reported, not set |
| `SubscribedClientCount` | How many remote devices have subscribed |
| `ClientPolicy` | The `MidiBluetoothPeripheralClientPolicy` for the next remote device that connects. It's reported whether or not the peripheral is running, because it's a rule, not the current state |
| `IsClientConnected` | True when a remote device is subscribed. This is the only sign that data can move |
| `EndpointDeviceId` | The MIDI endpoint's device interface id. The endpoint stands for the remote device, so it only exists while one is connected |
| `EndpointDeviceInstanceId` | The endpoint's instance id, which is what an endpoint customization matches on |
| `MessagesReceived` | How many messages have been received from the connected device |
| `MessagesSent` | How many messages have been sent to the connected device |
| `PacketsReceived` | How many Bluetooth packets have been received, counted before they're decoded. If packets keep climbing while `MessagesReceived` stays at zero, the connected device is sending something this transport can't decode |
| `PacketsSent` | How many Bluetooth packets have been sent to the connected device |
| `ConnectedClient` | The `MidiBluetoothPeripheralClient` that's connected, or null when nothing is |
| `AllowedClients` | The remembered allow decisions the service is using now, as `MidiBluetoothRememberedClient` entries |
| `DeniedClients` | The remembered deny decisions |

The service keeps `AllowedClients` and `DeniedClients`, but never saves them itself. Pass this whole status to `MidiBluetoothPeripheralClientListConfig` to save them. Otherwise, decisions made with `MidiBluetoothApprovalScope.Always` are lost the next time the service restarts.

The MIDI endpoint here stands for the remote device, the same way a Network MIDI 2.0 host endpoint stands for the remote client. There's nothing for an app to open until something connects.
