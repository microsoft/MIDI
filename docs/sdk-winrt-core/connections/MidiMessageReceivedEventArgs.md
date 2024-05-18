---
layout: api_page
title: MidiMessageReceivedEventArgs
parent: Connections
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# MidiMessageReceivedEventArgs

This is the main class to use when receving MIDI data from a message source such as a connection or a message processing plugin.

> Note: Do not keep a copy of the `MidiMessageReceivedEventArgs` class, as the data it points to is guaranteed to exist for only the duration of the event handler call for which this instance was an argument.

## Properties

| Property | Description |
| -------- | ----------- |
| `Timestamp` | The 64-bit MIDI Clock timestamp set by the service when this message was received |
| `PacketType` | Type of Universal MIDI Packet. This value can be cast to get the number of valid words in the data. You can  use this value to determine which of the `FillMessageXX` methods would be appropriate to call. For example, if the value is  `MidiPacketType.UniversalMidiPacket64` you would call `FillMessage64` |
| `MessageType` | The type of Universal MIDI Packet Message. This comes from the first 4 bits of the data. |

## Functions

| Function | Description |
| -------- | ----------- |
| `PeekFirstWord()` | Returns the first word of the message data without removing it. |
| `GetMessagePacket()` | Returns an `IMidiUniversalPacket` runtime class representing the data. This requires an allocation. |
| `FillWords(word0, word1, word2, word3)` | Puts the data in the supplied words and returns the number of valid words to read. If the return value is 2, for example, then only `word0` and `word1` contain valid data. |
| `FillMessageStruct(message)` | Fills the provided lightweight structure with the message data. Returns the number of valid words in the updated struct. |
| `FillMessage32(message)` | Adds the data to the provided MidiMessage32 runtimeclass. The reference behavior is projection-dependent. Returns true if the provided type matches the expected packet type and the data has been written. |
| `FillMessage64(message)` | Adds the data to the provided MidiMessage64 runtimeclass. The reference behavior is projection-dependent. Returns true if the provided type matches the expected packet type and the data has been written. |
| `FillMessage96(message)` | Adds the data to the provided MidiMessage96 runtimeclass. The reference behavior is projection-dependent. Returns true if the provided type matches the expected packet type and the data has been written. |
| `FillMessage128(message)` | Adds the data to the provided MidiMessage128 runtimeclass. The reference behavior is projection-dependent. Returns true if the provided type matches the expected packet type and the data has been written. |
| `FillWordArray(startIndex, words)`| Writes the data starting at the zero-based `startIndex`. Some projections pass a copy of all the data, so this may not always be an efficient approach. Returns the number of words written. |
| `FillByteArray(startIndex, bytes)`| Writes the data starting at the zero-based `startIndex`. Some projections pass a copy of all the data, so this may not always be an efficient approach. Returns the number of bytes written. |
| `FillBuffer(byteOffset, buffer)`| Writes the data to the buffer starting at byteOffset. Returns the number of bytes written. |
| `AppendWordsToList(wordList)`| Adds the message words to the end of the provided list, and returns the number of words added. |

## IDL

[MidiMessageReceivedEventArgs IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiMessageReceivedEventArgs.idl)
