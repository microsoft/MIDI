---
layout: sdk_reference_page
title: MidiBluetoothDeviceConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned when connecting a Bluetooth MIDI device
---

Returned in `MidiBluetoothDeviceConnectResponse`.

When the transport can't tell two causes apart, they share one value here, instead of the transport guessing. Then `ErrorMessage` has the transport's own, more specific wording, and `ErrorHResult` has the original error number.

The values are grouped by the step where a connection fails, which is why they skip around. `0x0000003x` is a bad request, `0x0000004x` is the transport itself, `0x000001xx` is the device, and `0x000003xx` is the radio.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0x00000000` | The device connected, or the request was saved for when it shows up |
| `UnrecognizedCommand` | `0x00000001` | The transport didn't recognize the command it was sent |
| `InvalidJson` | `0x00000011` | The configuration sent to the transport wasn't valid JSON |
| `MissingBluetoothDeviceId` | `0x00000031` | No Bluetooth device id was supplied |
| `InvalidBluetoothDeviceId` | `0x00000032` | The Bluetooth device id wasn't in the right format |
| `TransportNotAvailable` | `0x00000041` | The Bluetooth MIDI transport isn't running |
| `DeviceNotDiscovered` | `0x00000101` | The device has never been seen advertising, so there's nothing to connect to yet |
| `DeviceNotAvailable` | `0x00000102` | The device was found, but Windows couldn't open it |
| `MidiServiceNotFound` | `0x00000103` | It answered, but it has no Bluetooth MIDI service |
| `MidiCharacteristicNotFound` | `0x00000104` | It has the MIDI service, but neither a MIDI 1.0 nor a UMP characteristic |
| `DeviceUnreachable` | `0x00000105` | It didn't answer at all. It's usually asleep, out of range, or connected to another computer or phone |
| `GattAccessDenied` | `0x00000106` | Windows denied access to the device's GATT services |
| `GattProtocolError` | `0x00000107` | The GATT exchange with the device failed |
| `DeviceInUse` | `0x00000108` | Something else already has this device's MIDI service open. The older Bluetooth MIDI 1.0 support that comes with Windows takes over paired devices and keeps them to itself |
| `AlreadyConnected` | `0x00000109` | The device is already connected |
| `SessionCreationFailed` | `0x0000010A` | The device's MIDI session couldn't be created |
| `OperationAborted` | `0x0000010B` | The transport was shutting down, or the operation was canceled partway through |
| `NotifyFailed` | `0x0000010C` | The device wouldn't accept the subscription that delivers incoming MIDI |
| `EndpointCreationFailed` | `0x0000010D` | The connection worked, but the MIDI endpoint couldn't be created |
| `PairingRequired` | `0x0000010E` | The device refused until the connection is authenticated. Trying again can't work until someone pairs it, so the service stops trying this device until it's asked again |
| `GattTimeout` | `0x0000010F` | The device stopped answering partway through connecting. Some devices drop the connection until they're paired, so this and `PairingRequired` can describe the same device |
| `GattCallFailed` | `0x00000110` | Windows asked for the device's MIDI service or its characteristics, and the request failed right away instead of running out of time. `ErrorHResult` has the error number. This doesn't mean the device refused: some devices answer and the request still fails. Not the same as `GattTimeout`, which means the device went quiet |
| `RadioNotAvailable` | `0x00000301` | This PC has no Bluetooth radio it can use. See `MidiBluetoothRadioInformation` |
| `Unexpected` | `0x11002011` | Something unexpected went wrong |

