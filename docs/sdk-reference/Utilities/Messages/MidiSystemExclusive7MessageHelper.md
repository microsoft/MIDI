---
layout: sdk_reference_page
title: MidiSystemExclusive7MessageHelper
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class for working with MIDI 1.0 System Exclusive (SysEx 7) messages
---

This class provides helper functions for working with MIDI 1.0 System Exclusive (SysEx 7) messages encoded as 64-bit UMP Data Messages (`MidiMessage64` with message type `DataMessage64`).

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `GetDataBytesFromMultipleSystemExclusiveMessages(messages)` | Extracts and returns the SysEx data bytes from a collection of SysEx 7 UMP messages, assembling the full payload. |
| `GetDataBytesFromSingleSystemExclusiveMessage(message)` | Returns the SysEx data bytes from a single `MidiMessage64` SysEx 7 message. |
| `GetDataBytesFromSingleSystemExclusiveMessage(word0, word1)` | Returns the SysEx data bytes from the two raw words of a SysEx 7 message. |
| `AppendDataBytesFromSingleSystemExclusiveMessage(message, dataBytesToAppendTo)` | Appends the SysEx data bytes from a single `MidiMessage64` SysEx 7 message to the given collection. Returns the count of bytes appended. |
| `AppendDataBytesFromSingleSystemExclusiveMessage(word0, word1, dataBytesToAppendTo)` | Appends the SysEx data bytes from the two raw words of a SysEx 7 message to the given collection. Returns the count of bytes appended. |
| `GetDataByteCountFromSystemExclusiveMessageFirstWord(word0)` | Returns the number of valid data bytes indicated in the first word of a SysEx 7 message. |
| `MessageIsSystemExclusiveMessage(word0)` | Returns true if the first word's message type indicates a SysEx 7 message. |
