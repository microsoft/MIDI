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
| `Success` | `0x00000000` | The operation succeeded |
| `TransportNotAvailable` | `0x00000041` | The Bluetooth MIDI transport is not running |
| `AlreadyRunning` | `0x00000201` | This PC is already published |
| `NotRunning` | `0x00000202` | This PC is not published, so there was nothing to stop |
| `PeripheralRoleNotAvailable` | `0x00000203` | The radio would not publish the GATT service. Not every Bluetooth radio or driver supports the peripheral role, and everything else keeps working when this happens. |
| `NoClientConnected` | `0x00000204` | No remote device is subscribed |
| `InvalidProtocol` | `0x00000205` | A Bluetooth MIDI protocol must be chosen before this PC can be published |
| `AdvertisingFailed` | `0x00000206` | The radio would not start advertising |
| `RadioNotAvailable` | `0x00000301` | This PC has no usable Bluetooth radio. See `MidiBluetoothRadioInformation`. |

## Approving and denying a remote Central

These come back from `ApprovePeripheralClientAsync`, `DenyPeripheralClientAsync` and `ForgetPeripheralClientAsync`.

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `ClientNotPending` | `0x00000211` | No device with that address is waiting for a decision. It may have disconnected, or already been decided. |
| `ClientIdentityMismatch` | `0x00000212` | The device waiting at that address is not the one the decision was made about |
| `InvalidApprovalScope` | `0x00000213` | The requested `MidiBluetoothApprovalScope` cannot be applied to this device |
| `MissingClientAddress` | `0x00000214` | No client address was supplied |
| `ClientNotRemembered` | `0x00000215` | There is no remembered decision to forget for that address |
| `AddressNotRememberable` | `0x00000216` | The device's address rotates, so it cannot be recognized again and a permanent decision cannot be applied. Pairing the device fixes this. |
| `Unexpected` | `0x11002011` | An unexpected error occurred |

