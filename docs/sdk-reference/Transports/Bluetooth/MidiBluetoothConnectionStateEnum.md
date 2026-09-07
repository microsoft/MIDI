---
layout: sdk_reference_page
title: MidiBluetoothConnectionState
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: How far along a Bluetooth MIDI device is in getting connected
---

Reported by `MidiBluetoothDeviceInformation.ConnectionState`.

Connecting to a Bluetooth Low Energy device is not quick. The service opens a GATT session on a background worker, which takes seconds, and a device the customer has asked for stays wanted and is retried until it appears. `IsConnected` alone therefore cannot tell an attempt which is under way from a device which is simply switched off, which is why this exists.

Disconnecting a device in any state other than `NotConnected` cancels the request, so it is also the way to stop the service waiting for a device which is never coming back.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `NotConnected` | `0` | Not connected, and the service is not trying. A device which needs pairing also reports this, because retries are suspended until it is paired. |
| `WaitingForDevice` | `1` | Wanted, but the device has not been reachable yet. The service keeps retrying for as long as it stays wanted. |
| `Connecting` | `2` | An attempt is running right now. |
| `Connected` | `3` | Connected. |

An application showing a connect button will usually want to hide or disable it for anything other than `NotConnected`, and offer disconnect instead, so a customer cannot queue attempt after attempt while one is already in flight.
