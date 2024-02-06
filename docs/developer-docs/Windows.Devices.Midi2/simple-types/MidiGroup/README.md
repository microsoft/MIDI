# MidiGroup

The `MidiGroup` class is used to provide formatting and data validation for UMP (Universal MIDI Packet) groups.

## Properties

| Property | Description |
| --------------- | ----------- |
| Index | The data value, or group Index (0-15) |
| NumberForDisplay | The number that should be displayed in any UI. (1-16) |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| LabelShort | Returns the localized abbreviation. For example, "Gr" in English. |
| LabelFull | Returns the localized full name. For example, "Group" in English. |

## Functions

| Function | Description |
| --------------- | ----------- |
| MidiGroup() | Constructs an empty `MidiGroup` |
| MidiGroup(index) | Constructs a `MidiGroup` with the specified index |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| IsValidGroupIndex(index) | Verifies that the provided index is valid (between 0 and 15) |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiGroup.idl)
