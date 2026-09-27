---
layout: sdk_reference_page
title: IMidiUniversalPacket
namespace: Windows.Devices.Midi2
type: interface
description: Common interface implemented by the MidiMessageXX runtime classes.
---

The `MidiMessage32`, `MidiMessage64`, `MidiMessage96`, and `MidiMessage128` classes all implement this interface. You can also implement it on your own classes for particular kinds of messages, and then send them with `MidiEndpointConnection.SendSingleMessagePacket`.

## Properties

| Property | Description |
| -------- | ----------- |
| `Timestamp` | The 64-bit timestamp. For an incoming message, the service sets it when the message arrives. For an outgoing message, you set it to say when the message should be sent |
| `MessageType` | A [MidiMessageType]({{ site.baseurl }}/sdk-reference/MidiMessageTypeEnum/) value: the 4-bit message type, from 0x0 to 0xF, as defined in the UMP specification |
| `PacketType` | A [MidiPacketType]({{ site.baseurl }}/sdk-reference/MidiPacketTypeEnum/) value. Its number value is the count of 32-bit words in the packet |

## Functions

| Function | Description |
| -------- | ----------- |
| `PeekFirstWord()` | Returns the first word of the message. You can call it before you know the message's type or size |
| `GetAllWords()` | Returns all the message's words as a list of 32-bit words |
| `AppendAllMessageWordsToList(targetList)` | Adds all the message's words to the end of the list, and returns how many were added |
| `FillBuffer(byteOffset, buffer)` | Copies the message's bytes into the buffer, starting at `byteOffset`, and returns how many bytes were written |
