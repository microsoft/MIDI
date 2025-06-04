---
layout: sdk_reference_page
title: Midi1PortNameTableEntry
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Represents the names generated when constructing a MIDI 1.0 port from a UMP device
---

This table is generated when the parent UMP device is enumerated, and is then stored in a property on the parent UMP endpoint. The table is later used to set the FriendlyName on the Software Device (SWD) which represents the MIDI 1.0 port. 

A list of these structs is created by functions on the `MidiEndpointDeviceInformation` class.

Once the MIDI 1.0 port has been created, this information is not of any real use, other than to present in the Settings app when the user wants to pick a generated name, or supply a custom name, for the port for the next time it is created.

## Fields

| Field | Description
| --- | --- |
| `GroupIndex` | 0-based index of the associated UMP group  |
| `Flow` | a `Midi1PortFlow` value indicating the direction for this name |
| `CustomName` | A user-supplied custom name, if any |
| `LegacyWinMMName` | A name compatible with the WinMM MIDI API |
| `PinName` | The name generated from the KS Pin (USB iJack) on the device |
| `FilterPlusPinName` | The name generated from the filter (USB interface) plus pin on the device |
| `BlockName` | The name generated for the Group Terminal Block |
| `FilterPlusBlockName` | A name that includes the filter plus the Group Terminal Block name |
