# MidiUniqueId

The `MidiUniqueId` class is used to provide formatting and data validation for MIDI-CI MUID types used in Function Blocks and MIDI CI transactions.

## Properties

| Property | Description |
| --------------- | ----------- |
| Byte1 | The data value for byte 1 of the MUID |
| Byte2 | The data value for byte 2 of the MUID |
| Byte3 | The data value for byte 3 of the MUID |
| Byte4 | The data value for byte 4 of the MUID |
| As28BitInteger | The data value converted to a 28 bit integer |
| IsBroadcast | True if this is the  broadcast MUID value |
| IsReserved | True if this is the reserved MUID value |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| LabelShort | Returns the localized abbreviation. |
| LabelFull | Returns the localized full name. |

## Functions

| Function | Description |
| --------------- | ----------- |
| MidiUniqueId() | Constructs an empty `MidiUniqueId` |
| MidiUniqueId(integer28bit) | Constructs the `MidiUniqueId` from the given 28 bit integer |
| MidiUniqueId(byte1, byte2, byte3, byte4) | Constructs a `MidiUniqueId` with the specified bytes |

## Static Functions

| Function | Description |
| --------------- | ----------- |
| CreateBroadcast() | Constructs a broadcast `MidiUniqueId` |


[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiUniqueId.idl)
