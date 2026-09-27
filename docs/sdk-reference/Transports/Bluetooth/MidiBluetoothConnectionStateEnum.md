---
layout: sdk_reference_page
title: MidiBluetoothConnectionState
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How far along a Bluetooth MIDI device is in getting connected
---

Reported by `MidiBluetoothDeviceInformation.ConnectionState`.

Connecting to a Bluetooth Low Energy device isn't quick. The service connects in the background, which takes a few seconds. And a device someone asked for keeps being tried until it shows up. So `IsConnected` alone can't tell a connection that's in progress from a device that's just turned off. That's why this exists.

Disconnecting a device in any state other than `NotConnected` cancels the request. So that's also how you stop the service from waiting for a device that's never coming back.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NotConnected` | `0` | Not connected, and the service isn't trying. A device that needs pairing also reports this, because the service stops trying until it's paired |
| `WaitingForDevice` | `1` | Someone asked to connect, but the device hasn't been reachable yet. The service keeps trying until the request is canceled |
| `Connecting` | `2` | The service is trying to connect right now |
| `Connected` | `3` | Connected |

An app with a connect button should usually hide or turn off that button for anything other than `NotConnected`, and offer disconnect instead. That way, people can't pile up connection requests while one is already in progress.
