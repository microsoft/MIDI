---
layout: sdk_reference_page
title: MidiMessageHelper
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Class with helper functions for UMP messages
---

This class has static helper functions that read and change the fields in Universal MIDI Packets. It also has functions that turn packet data into text that people can read.

Most of the time, your app needs to check a message before it calls a function that returns a specific field. For example, if you ask for the Flex Data status but don't pass in a Flex Data message, the function just returns whatever data happens to be where that field would be.

## Checking message types

| Static Method | Description |
| --------------- | ----------- |
| `ValidateMessage32MessageType(word0)` | Returns true if the message type in this word is for a 32-bit UMP |
| `ValidateMessage64MessageType(word0)` | Returns true if the message type in this word is for a 64-bit UMP |
| `ValidateMessage96MessageType(word0)` | Returns true if the message type in this word is for a 96-bit UMP |
| `ValidateMessage128MessageType(word0)` | Returns true if the message type in this word is for a 128-bit UMP |

## Which fields a message type has

| Static Method | Description |
| --------------- | ----------- |
| `MessageTypeHasGroupField(messageType)` | Returns true if this message type has a group field. It only knows about message types that existed when the API was written |
| `MessageTypeHasChannelField(messageType)` | Returns true if this message type has a channel field. It only knows about message types that existed when the API was written |

## Reading fields

| Static Method | Description |
| --------------- | ----------- |
| `GetMessageTypeFromMessageFirstWord(word0)` | Returns the message's `MidiMessageType` |
| `GetPacketTypeFromMessageFirstWord(word0)` | Returns the message's `MidiPacketType` |
| `GetGroupFromMessageFirstWord(word0)` | Returns the message's `MidiGroup`. Check first that the message type has a group field |
| `GetChannelFromMessageFirstWord(word0)` | Returns the message's `MidiChannel`. Check first that the message type has a channel field |
| `GetStatusFromUtilityMessage(word0)` | Returns the status byte |
| `GetStatusFromMidi1ChannelVoiceMessage(word0)` | For a MIDI 1.0 channel voice message, returns its `Midi1ChannelVoiceMessageStatus` |
| `GetStatusFromMidi2ChannelVoiceMessageFirstWord(word0)` | For a MIDI 2.0 channel voice message, returns its `Midi2ChannelVoiceMessageStatus` |
| `GetStatusBankFromFlexDataMessageFirstWord(word0)` | Returns the status bank byte |
| `GetStatusFromFlexDataMessageFirstWord(word0)` | Returns the status byte |
| `GetStatusFromSystemCommonMessage(word0)` | Returns the status byte |
| `GetStatusFromDataMessage64FirstWord(word0)` | Returns the status byte |
| `GetNumberOfBytesFromDataMessage64FirstWord(word0)` | Returns the number of bytes the message says it holds |
| `GetStatusFromDataMessage128FirstWord(word0)` | Returns the status byte |
| `GetNumberOfBytesFromDataMessage128FirstWord(word0)` | Returns the number of bytes the message says it holds |
| `GetFormFromStreamMessageFirstWord(word0)` | Returns the 4-bit form field, as a byte |
| `GetStatusFromStreamMessageFirstWord(word0)` | Returns the status byte |

## Changing fields

| Static Method | Description |
| --------------- | ----------- |
| `ReplaceGroupInMessageFirstWord(word0, newGroup)` | Returns `word0` with its group field changed to `newGroup` |
| `ReplaceChannelInMessageFirstWord(word0, newChannel)` | Returns `word0` with its channel field changed to `newChannel` |

## Words and packets

| Static Method | Description |
| --------------- | ----------- |
| `GetPacketListFromWordList(timestamp, words)` | Creates a list of Universal MIDI Packets from these MIDI words |
| `GetWordListFromPacketList(messages)` | Returns a list of the MIDI words in all of these Universal MIDI Packets, in order |

## Display names

| Static Method | Description |
| --------------- | ----------- |
| `GetMessageDisplayNameFromFirstWord(word0)` | Returns a friendly name for the message. It's what the MIDI Console shows when you monitor an endpoint in verbose mode |
| `GetNoteDisplayNameFromNoteIndex(noteIndex)` | Returns the usual name for a MIDI note number from 0 to 127 |
| `GetNoteOctaveFromNoteIndex(noteIndex)` | Returns the octave number of a MIDI note number, using the default octave for middle C |
| `GetNoteOctaveFromNoteIndex(noteIndex, middleCOctave)` | Returns the octave number of a MIDI note number, using `middleCOctave` as the octave for middle C |
