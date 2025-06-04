---
layout: sdk_reference_page
title: MidiMessageHelper
namespace: Microsoft.Windows.Devices.Midi2.Messages
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Class with helper functions for UMP messages
---

This class contains a number of static helper functions for reading information from Universal MIDI Packets, and also manipulating that information. It also contains some utility functions for getting human-readable information from the data in a packet.

In most cases, the calling application needs to do some validation before calling functions which return specific fields. If, for example, the application asks for the Flex Data Status, but doesn't provide a valid Flex Data message, the function will happily return whatever other data is in the position of that field.

## Validation Functions

| Function | Description |
| --------------- | ----------- |
| `ValidateMessage32MessageType (word0)`| Validate that the message type field in the word is for a 32-bit UMP |
| `ValidateMessage64MessageType (word0)` | Validate that the message type field in the word is for a 64-bit UMP  |
| `ValidateMessage96MessageType (word0)` | Validate that the message type field in the word is for a 96-bit UMP  |
| `ValidateMessage128MessageType (word0)` | Validate that the message type field in the word is for a 128-bit UMP  |

## Informational Functions

| Function | Description |
| --------------- | ----------- |
| `MessageTypeHasGroupField (messageType)` | Returns true if the message type is known to be one which contains a group field. Valid only for message types known at the type the API was written. |
| `MessageTypeHasChannelField (messageType)` | Returns true if the message type is known to be one which contains a channel field. Valid only for message types known at the type the API was written. |

## Field Access Functions

| Function | Description |
| --------------- | ----------- |
| `GetMessageTypeFromMessageFirstWord (word0)` | Returns the `MidiMessageType` for the message |
| `GetPacketTypeFromMessageFirstWord (word0)` | Returns the `MidiPacketType` for the message |
| `GetGroupFromMessageFirstWord (word0)` | Returns the `MidiGroup` for the message. First check to see if the message type has a group field. |
| `GetChannelFromMessageFirstWord (word0)` | Returns the `MidiChannel` for the message. First check to see if the message type has a channel field. |
| `GetStatusFromUtilityMessage (word0)` | Returns the status byte |
| `GetStatusFromMidi1ChannelVoiceMessage (word0)` | When provided a MIDI 1.0 channel voice message, returns the `Midi1ChannelVoiceMessageStatus` for the message. |
| `GetStatusFromMidi2ChannelVoiceMessageFirstWord (word0)` | When provided a MIDI 2.0 channel voice message, returns the `Midi2ChannelVoiceMessageStatus` for the message. |
| `GetStatusBankFromFlexDataMessageFirstWord (word0)` | Returns the status bank byte |
| `GetStatusFromFlexDataMessageFirstWord (word0)` | Returns the status byte |
| `GetStatusFromSystemCommonMessage (word0)` | Returns the status byte |
| `GetStatusFromDataMessage64FirstWord (word0)` | Returns the status byte |
| `GetNumberOfBytesFromDataMessage64FirstWord (word0)` | Returns the byte count field |
| `GetStatusFromDataMessage128FirstWord (word0)` | Returns the status byte |
| `GetNumberOfBytesFromDataMessage128FirstWord (word0)` | Returns the byte count field |
| `GetFormFromStreamMessageFirstWord (word0)` | Returns the form nibble as a byte |
| `GetStatusFromStreamMessageFirstWord (word0)` | Returns the status byte |

## Field Manipulation Functions

| Function | Description |
| --------------- | ----------- |
| `ReplaceGroupInMessageFirstWord (word0, newGroup)` | Replaces the group field in word0 and returns the resulting MIDI word |
| `ReplaceChannelInMessageFirstWord (word0, newChannel)` | Replaces the channel field in word0 and returns the resulting MIDI word |

## UMP Data format conversion

| Function | Description |
| --------------- | ----------- |
| `GetPacketListFromWordList (timestamp, words)` | Creates a list of Universal MIDI Packets from the supplied MIDI words |
| `GetWordListFromPacketList (packets)` | Returns a list of MIDI words from all the supplied Universal MIDI Packets, in order |

## Additional Functions

| Function | Description |
| --------------- | ----------- |
| `GetMessageDisplayNameFromFirstWord (word0)` | Returns the "Friendly Name" string for a message. To see an example, this is what is displayed in the MIDI console output when you are monitoring an endpoint in verbose mode. |
| `GetNoteDisplayNameFromNoteIndex (noteIndex)` | Returns the common display name for a MIDI note index 0-127 |
| `GetNoteOctaveFromNoteIndex (noteIndex, middleCOctave)` | Returns the octave number for a MIDI note index |
