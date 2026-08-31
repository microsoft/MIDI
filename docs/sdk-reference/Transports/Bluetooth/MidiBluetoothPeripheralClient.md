---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClient
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The remote device connected to this PC while it is published as a Bluetooth MIDI peripheral
---

The remote Central which has connected to this PC. This is the opposite direction to `MidiBluetoothDeviceInformation`, so the identifiers differ: the remote chose the connection parameters, and Windows supplies a device interface id for it rather than an address being used as the key.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The name the remote device reports. |
| `HasGenericName` | True when the remote reports something like "iPhone" rather than a distinguishing name. Phones and tablets withhold their real name from an unpaired PC. |
| `BluetoothAddress` | The remote's Bluetooth address. |
| `BluetoothAddressType` | Whether that address is public or random. |
| `IsPaired` | True when the remote is paired with this PC. |
| `IsRememberable` | False when the device's address rotates for privacy, which means it cannot be recognized again and `MidiBluetoothApprovalScope.Always` cannot be used for it. Pairing the device makes it rememberable. |
| `ApprovalRequestedTime` | When this device started waiting for a decision. Zero for a client which is already connected, because nothing is waiting on it. |
| `WindowsDeviceId` | A Windows device interface id, unlike the address-based ids used elsewhere in this namespace. |
| `ConnectionInterval` | The interval the remote asked for when it connected. |

An unpaired device has no stable identity, so any endpoint customization applied to it will apply to whichever unpaired device connects next. Pairing has to be initiated from the remote device, since that is the side which scanned for and connected to this PC.
