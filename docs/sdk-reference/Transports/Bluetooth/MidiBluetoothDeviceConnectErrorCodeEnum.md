---
layout: sdk_reference_page
title: MidiBluetoothDeviceConnectErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned when connecting a Bluetooth MIDI device
---

Returned in `MidiBluetoothDeviceConnectResponse`.

Causes the transport cannot tell apart share a value here rather than being guessed at. When that happens, `ErrorMessage` carries the transport's own wording, which is more specific, and `ErrorHResult` carries the raw value.

The values are grouped by the stage a connection fails at, which is why they are not consecutive. `0x0000003x` is a bad request, `0x0000004x` is the transport itself, `0x000001xx` is the device, and `0x000003xx` is the radio.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0x00000000` | The device connected, or the request was remembered for when it appears |
| `UnrecognizedCommand` | `0x00000001` | The transport did not recognize the command it was sent |
| `InvalidJson` | `0x00000011` | The configuration sent to the transport was not valid JSON |
| `MissingBluetoothDeviceId` | `0x00000031` | No Bluetooth device id was supplied |
| `InvalidBluetoothDeviceId` | `0x00000032` | The Bluetooth device id was malformed |
| `TransportNotAvailable` | `0x00000041` | The Bluetooth MIDI transport is not running |
| `DeviceNotDiscovered` | `0x00000101` | The device has never been seen advertising, so there is nothing to connect to yet |
| `DeviceNotAvailable` | `0x00000102` | Discovered, but Windows could not open it |
| `MidiServiceNotFound` | `0x00000103` | It answered, but it has no Bluetooth MIDI service |
| `MidiCharacteristicNotFound` | `0x00000104` | It has the MIDI service, but neither a MIDI 1.0 nor a UMP characteristic |
| `DeviceUnreachable` | `0x00000105` | It did not answer at all. Usually asleep, out of range, or connected to another host. |
| `GattAccessDenied` | `0x00000106` | Windows denied access to the device's GATT services |
| `GattProtocolError` | `0x00000107` | The GATT exchange failed |
| `DeviceInUse` | `0x00000108` | Something else already holds this device's MIDI service open. The older in-box Windows Bluetooth MIDI 1.0 support claims paired devices and holds them exclusively. |
| `AlreadyConnected` | `0x00000109` | The device is already connected |
| `SessionCreationFailed` | `0x0000010A` | The device's MIDI session could not be created |
| `OperationAborted` | `0x0000010B` | The transport was shutting down, or the operation was canceled part way through |
| `NotifyFailed` | `0x0000010C` | The device would not accept the subscription which delivers incoming MIDI |
| `EndpointCreationFailed` | `0x0000010D` | The connection succeeded but the MIDI endpoint could not be created |
| `RadioNotAvailable` | `0x00000301` | This PC has no usable Bluetooth radio. See `MidiBluetoothRadioInformation`. |
| `Unexpected` | `0x11002011` | An unexpected error occurred |

