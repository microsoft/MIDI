---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClient
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: The remote device connected to this PC while it is published as a Bluetooth MIDI peripheral
---

The remote device, such as a phone or tablet, that connected to this PC. This is the opposite direction from `MidiBluetoothDeviceInformation`, so the ids are different. The remote device chose the connection settings, and Windows gives it a device interface id, instead of using its address as the key.

## Properties

| Property | Description |
| -------- | ----------- |
| `Name` | The name the remote device reports |
| `HasGenericName` | True when the remote device reports a general name like "iPhone" instead of one that tells it apart. Phones and tablets hide their real name from a PC they aren't paired with |
| `BluetoothAddress` | The remote device's Bluetooth address |
| `BluetoothAddressType` | Whether that address is public or random |
| `IsPaired` | True when the remote device is paired with this PC |
| `IsRememberable` | False when the device changes its address for privacy. Then it can't be recognized again, and `MidiBluetoothApprovalScope.Always` can't be used for it. Pairing the device makes it rememberable |
| `ApprovalRequestedTime` | When this device started waiting for a decision. Zero for a device that's already connected, because nothing is waiting on it |
| `WindowsDeviceId` | A Windows device interface id, unlike the address-based ids used elsewhere in this namespace |
| `ConnectionInterval` | The time between data exchanges that the remote device asked for when it connected |

A device that isn't paired has no identity that stays the same. So an endpoint customization you apply to it applies to whichever unpaired device connects next. Pairing has to be started from the remote device, because that's the side that found and connected to this PC.
