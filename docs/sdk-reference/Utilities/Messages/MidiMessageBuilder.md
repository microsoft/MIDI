---
layout: sdk_reference_page
title: MidiMessageBuilder
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class to construct common message types
---

This class builds Universal MIDI Packets (UMPs) from their parts, so you don't have to shift and mask the bits yourself. Each function returns a message object you can send with `MidiEndpointConnection.SendSingleMessagePacket`. If something goes wrong while building, the function returns null.

Every function takes a `timestamp` first. Pass `0` or `MidiClock.TimestampConstantSendImmediately` to send the message right away.

A value that is too big for its field is trimmed to fit instead of being rejected. For example, a 7-bit data byte keeps only its lower 7 bits. Check your values before building, because a trimmed value is sent without any warning.

Two other classes have easier functions for some of these messages. [`MidiSystemExclusive7MessageBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiSystemExclusive7MessageBuilder) builds a whole System Exclusive message from a list of bytes, and [`MidiStreamMessageBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Messages/MidiStreamMessageBuilder) builds the stream messages endpoints use to describe themselves.

## Static Functions

| Function | Description |
| -------- | ----------- |
| `BuildUtilityMessage(timestamp, status, dataOrReserved)` | Builds a 32-bit utility message (message type 0), such as a jitter reduction timestamp. `status` is 4 bits and `dataOrReserved` is 20 bits. Utility messages don't belong to a group |
| `BuildSystemMessage(timestamp, group, status, midi1Byte2, midi1Byte3)` | Builds a 32-bit system common or system real-time message (message type 1), such as Timing Clock or Song Position Pointer. `status` is the MIDI 1.0 status byte, for example `0xF8`. The two data bytes are 7 bits each. Pass `0` for any the message doesn't use |
| `BuildMidi1ChannelVoiceMessage(timestamp, group, status, channel, byte3, byte4)` | Builds a 32-bit MIDI 1.0 channel voice message (message type 2), such as Note On or Control Change. `byte3` and `byte4` are the two MIDI 1.0 data bytes, 7 bits each. For a Note On, they are the note number and the velocity |
| `BuildSystemExclusive7Message(timestamp, group, status, numberOfBytes, dataByte0, dataByte1, dataByte2, dataByte3, dataByte4, dataByte5)` | Builds one 64-bit System Exclusive 7 packet (message type 3), which holds up to six data bytes. `status` says whether this packet is the whole message (`0`), the start (`1`), a middle part (`2`), or the end (`3`). `numberOfBytes` is how many of the six data bytes are used |
| `BuildMidi2ChannelVoiceMessage(timestamp, group, status, channel, index, data)` | Builds a 64-bit MIDI 2.0 channel voice message (message type 4). `index` fills the lower 16 bits of the first word. For a Note On, that's the note number and the attribute type. `data` is the whole second word. For a Note On, that's the velocity and the attribute data |
| `BuildSystemExclusive8Message(timestamp, group, status, numberOfValidDataBytesThisMessage, streamId, dataByte00, ..., dataByte12)` | Builds one 128-bit System Exclusive 8 packet (message type 5), which holds up to 13 data bytes. `status` is a `MidiSystemExclusive8Status` value that says which part of the message this packet is. `numberOfValidDataBytesThisMessage` is written into the packet's byte count as given. The UMP specification counts the stream id as one of those bytes |
| `BuildMixedDataSetChunkHeaderMessage(timestamp, group, mdsId, numberValidDataBytesInThisChunk, numberChunksInMixedDataSet, numberOfThisChunk, manufacturerId, deviceId, subId1, subId2)` | Builds the header packet that starts each chunk of a Mixed Data Set (message type 5, status 8). A Mixed Data Set carries a large block of data. `mdsId` ties together the packets of one data set |
| `BuildMixedDataSetChunkDataMessage(timestamp, group, mdsId, dataByte00, ..., dataByte13)` | Builds a data packet for a Mixed Data Set chunk (message type 5, status 9). Each one holds 14 data bytes |
| `BuildFlexDataMessage(timestamp, group, form, address, channel, statusBank, status, word1Data, word2Data, word3Data)` | Builds a 128-bit Flex Data message (message type 0xD), used for things like tempo, time signature, and lyrics. `form` and `address` are 2 bits each. `statusBank` and `status` pick the kind of message |
| `BuildStreamMessage(timestamp, form, status, word0RemainingData, word1Data, word2Data, word3Data)` | Builds a 128-bit stream message (message type 0xF). `form` is 2 bits and `status` is 10 bits. Stream messages don't belong to a group. `MidiStreamMessageBuilder` has an easier function for each stream message |
