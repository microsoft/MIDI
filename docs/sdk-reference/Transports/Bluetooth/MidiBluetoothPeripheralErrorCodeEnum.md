---
layout: sdk_reference_page
title: MidiBluetoothPeripheralErrorCode
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: enum
description: Error codes returned by Bluetooth MIDI peripheral operations
---

Returned in `MidiBluetoothPeripheralResponse` and in `MidiBluetoothPeripheralClientDecisionResponse`.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Success` | `0x00000000` | It worked |
| `TransportNotAvailable` | `0x00000041` | The Bluetooth MIDI transport isn't running |
| `AlreadyRunning` | `0x00000201` | This PC is already published |
| `NotRunning` | `0x00000202` | This PC isn't published, so there was nothing to stop |
| `PeripheralRoleNotAvailable` | `0x00000203` | The radio wouldn't publish the GATT service. Not every Bluetooth radio or driver supports the peripheral role. Everything else keeps working when this happens |
| `NoClientConnected` | `0x00000204` | No remote device is subscribed |
| `InvalidProtocol` | `0x00000205` | You have to choose a Bluetooth MIDI protocol before this PC can be published |
| `AdvertisingFailed` | `0x00000206` | The radio wouldn't start advertising |
| `RadioNotAvailable` | `0x00000301` | This PC has no Bluetooth radio it can use. See `MidiBluetoothRadioInformation` |

## Approving and denying a remote device

These come back from `ApprovePeripheralClientAsync`, `DenyPeripheralClientAsync`, and `ForgetPeripheralClientAsync`.

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `ClientNotPending` | `0x00000211` | No device with that address is waiting for a decision. It may have disconnected, or someone already decided |
| `ClientIdentityMismatch` | `0x00000212` | The device waiting at that address isn't the one the decision was about |
| `InvalidApprovalScope` | `0x00000213` | The `MidiBluetoothApprovalScope` you asked for can't be used for this device |
| `MissingClientAddress` | `0x00000214` | No client address was supplied |
| `ClientNotRemembered` | `0x00000215` | There's no remembered decision to forget for that address |
| `AddressNotRememberable` | `0x00000216` | The device changes its address, so it can't be recognized again, and a permanent decision can't be applied. Pairing the device fixes this |
| `Unexpected` | `0x11002011` | Something unexpected went wrong |

