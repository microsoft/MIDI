# MidiChannel

The MidiChannel class is used to provide formatting and data validation for MIDI 1.0 and MIDI 2.0 channels.

## Properties

| Property | Description |
| --------------- | ----------- |
| Index | The data value, or channel Index (0-15) |
| NumberForDisplay | The number that should be displayed in any UI. (1-16) |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| LabelShort | Returns the localized abbreviation. For example, "Ch" in English. |
| LabelFull | Returns the localized full name. For example, "Channel" in English. |

## Functions

| Function | Description |
| --------------- | ----------- |
| MidiChannel() | Constructs an empty `MidiChannel` |
| MidiChannel(index) | Constructs a `MidiChannel` with the specified index |

## Static Functions

| Static Function | Description |
| --------------- | ----------- |
| IsValidChannelIndex(index) | Verifies that the provided index is valid (between 0 and 15) |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiChannel.idl)
