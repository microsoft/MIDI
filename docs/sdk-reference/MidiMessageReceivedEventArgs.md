---
layout: sdk_reference_page
title: MidiMessageReceivedEventArgs
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Argument supplied when an incoming MIDI message is received
---

Every incoming message reaches you as one of these, whether it comes from a connection or from a message processing plugin.

> **Note:** Don't hold on to a `MidiMessageReceivedEventArgs` after your event handler returns. The data it points to exists only while the handler runs. If you need the message later, copy it out first, for example with `GetMessagePacket()` or `FillMessageStruct()`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Timestamp` | The `MidiClock` timestamp the service gave the message when it arrived |
| `PacketType` | The size of the packet. Its number value is the count of valid words. Use it to pick which `FillMessage` function to call. For example, if it's `MidiPacketType.UniversalMidiPacket64`, call `FillMessage64` |
| `MessageType` | The message type, from the first 4 bits of the message |

## Functions

| Function | Description |
| -------- | ----------- |
| `PeekFirstWord()` | Returns the first word of the message |
| `GetMessagePacket()` | Returns the message as a new `IMidiUniversalPacket` object. It creates a new object every time, which takes a little extra time |
| `FillWords(word0, word1, word2, word3)` | Copies the message into the four words you pass, and returns how many of them are valid. For example, if it returns 2, only `word0` and `word1` hold message data |
| `FillMessageStruct(message)` | Copies the message into a `MidiMessageStruct`, and returns how many of its words are valid |
| `FillMessage32(message)` | Copies the message into the `MidiMessage32` you pass. Returns true if the message is the right size for that class and was copied |
| `FillMessage64(message)` | Copies the message into the `MidiMessage64` you pass. Returns true if the message is the right size for that class and was copied |
| `FillMessage96(message)` | Copies the message into the `MidiMessage96` you pass. Returns true if the message is the right size for that class and was copied |
| `FillMessage128(message)` | Copies the message into the `MidiMessage128` you pass. Returns true if the message is the right size for that class and was copied |
| `FillWordArray(startIndex, words)`| Copies the message's words into the array, starting at the zero-based `startIndex`, and returns how many words were written. Some languages copy the whole array to make this call, so it may not be the fastest choice |
| `FillByteArray(startIndex, bytes)`| Copies the message's bytes into the array, starting at the zero-based `startIndex`, and returns how many bytes were written. Some languages copy the whole array to make this call, so it may not be the fastest choice |
| `FillBuffer(byteOffset, buffer)`| Copies the message's bytes into the buffer, starting at `byteOffset`, and returns how many bytes were written |
| `AppendWordsToList(wordList)`| Adds the message's words to the end of the list, and returns how many were added |
