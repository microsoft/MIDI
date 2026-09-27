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
| `BluetoothDeviceId` | The device's Bluetooth address as twelve hex digits, and the key for every operation in this namespace. It isn't a Windows device interface id |
| `BluetoothAddress` | The same address as a number, to match up with the `Windows.Devices.Bluetooth` APIs |
| `Name` | The name the device reports. Empty until the device has been heard long enough to learn it |
| `SelectedProtocol` | The `MidiBluetoothProtocol` in use. `Unknown` until the device is connected, because finding out means reading the device's characteristics |
| `IsConnected` | True when the device is connected to this PC |
| `ConnectionState` | A `MidiBluetoothConnectionState` that says how far along the device is. Connecting happens in the background, and a device someone asked for keeps being tried until it shows up, so this tells you more than `IsConnected` can |
| `IsPaired` | True when the device is paired with this PC. Bluetooth MIDI doesn't require pairing |
| `RequiresPairing` | True when the device won't provide its MIDI service until the connection is authenticated. A device's advertising never says this, so it's only known after trying to connect. While it's true, the service stops trying the device, because every try brings up another Windows pairing prompt |
| `IsPresent` | True while the device is advertising. Bluetooth MIDI devices go to sleep quickly to save power, so a device that isn't present is usually asleep, not gone |
| `SignalStrengthDecibelMilliwatts` | The signal strength of the most recent advertisement, in dBm. A connected device stops advertising, so then this is out of date |
| `LastSeenAgo` | How long ago the device was last heard. Means nothing when `HasBeenSeen` is false |
| `HasBeenSeen` | False when the radio has never heard this device at all. This is how you tell a paired device that Windows remembers from one that was heard a long time ago |
| `HasEndpoint` | True when a MIDI endpoint exists for this device |
| `EndpointDeviceId` | The MIDI endpoint's device interface id, if there is one |
| `EndpointDeviceInstanceId` | The endpoint's instance id, which is what an endpoint customization matches on |
| `MessagesReceived` | How many messages have been received from the device |
| `MessagesSent` | How many messages have been sent to the device |
| `PacketsReceived` | How many Bluetooth packets have been received, counted before they're decoded. If packets keep climbing while `MessagesReceived` stays at zero, the device is sending something this transport can't decode. If both are zero, it isn't sending anything at all |
| `PacketsSent` | How many Bluetooth packets have been sent |
| `TimestampSource` | A `MidiBluetoothTimestampSource` that says whether the device's own timestamps are used, or the time each message arrived is used instead |
| `ConnectionInterval` | The time between data exchanges that the PC and the device agreed on. Zero when not connected |
| `LastConnectError` | The transport's own wording for the most recent connection failure. It's more specific than the error code |
| `LastConnectErrorCode` | A `MidiBluetoothDeviceConnectErrorCode` that says why the last connection attempt failed |
| `LastConnectErrorHResult` | The HRESULT error number behind that failure |
| `LastSendErrorHResult` | The HRESULT error number from the most recent failed send |
| `OfflineRetentionSeconds` | How long this device's endpoint stays after the device goes offline, as seconds or a named `MidiBluetoothOfflineRetention` value. This is the device's own setting, so it can be `UseTransportDefault` |
| `EffectiveOfflineRetentionSeconds` | The same, after the transport setting is applied, so it's never `UseTransportDefault`. Show this one to people |

Check `LastConnectErrorCode` after you ask for a connection. Connecting happens in the background, long after `ConnectDeviceAsync` returns. So a failure during that work shows up here, not in the response.

The endpoint's native data format isn't reported here. Read it from the endpoint itself, with `MidiEndpointDeviceInformation` and `Windows.Devices.Midi2.Enumeration.MidiEndpointNativeDataFormat`.
