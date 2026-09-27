---
layout: sdk_reference_page
title: MidiSystemExclusive7MessageHelper
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class for working with MIDI 1.0 System Exclusive (SysEx 7) messages
---

Helper functions for MIDI 1.0 System Exclusive (SysEx 7) messages, which UMP carries as 64-bit data messages (`MidiMessage64` with message type `DataMessage64`).

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `GetDataBytesFromMultipleSystemExclusiveMessages(messages)` | Pulls the SysEx data bytes out of a list of SysEx 7 UMP messages, and joins them into the full data |
| `GetDataBytesFromSingleSystemExclusiveMessage(message)` | Returns the SysEx data bytes from one `MidiMessage64` SysEx 7 message |
| `GetDataBytesFromSingleSystemExclusiveMessage(word0, word1)` | Returns the SysEx data bytes from the two words of a SysEx 7 message |
| `AppendDataBytesFromSingleSystemExclusiveMessage(message, dataBytesToAppendTo)` | Adds the SysEx data bytes from one `MidiMessage64` SysEx 7 message to the end of `dataBytesToAppendTo`. Returns how many bytes it added |
| `AppendDataBytesFromSingleSystemExclusiveMessage(word0, word1, dataBytesToAppendTo)` | Adds the SysEx data bytes from the two words of a SysEx 7 message to the end of `dataBytesToAppendTo`. Returns how many bytes it added |
| `GetDataByteCountFromSystemExclusiveMessageFirstWord(word0)` | Returns how many data bytes the first word of a SysEx 7 message says it holds |
| `MessageIsSystemExclusiveMessage(word0)` | Returns true if the first word's message type is SysEx 7 |
| `VerifyContainsOnlyDataBytes(dataBytesToTest)` | Returns true when every byte has its high bit clear, which makes it a data byte. SysEx 7 only carries data bytes, so check your data with this before you pack it into messages |
