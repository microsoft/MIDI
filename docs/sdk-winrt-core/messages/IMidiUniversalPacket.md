---
layout: api_page
title: IMidiUniversalPacket
parent: Messages
grand_parent: Microsoft.Devices.Midi2
has_children: false
---

# IMidiUniversalPacket

This interface is implemented by the rich MidiMessageXX runtime class types. It may also be used as the interface for message-specific classes you create yourself.

## Properties

| Property | Description |
| -------- | ----------- |
| `Timestamp` | 64 bit timestamp set by the receiving transport in the case of incoming messages, or by the sender in the case of outgoing messages |
| `MessageType` | A [MidiMessageType enumeration value](./MidiMessageTypeEnum.md) which represents the 4 bit MIDI Message type 0x0 - 0xF as defined by the MIDI UMP standard. |
| `PacketType` | A [MidiPacketType enumeration value](./MidiPacketTypeEnum.md) which can be cast to an int to get the number of 32-bit words in the message packet |

## Functions

| Function | Description |
| -------- | ----------- |
| `PeekFirstWord()` | Provides access to the first word of data, even if the message type and size is not yet known by the API user |
| `GetAllWords()` | Returns all the message words as a list of 32-bit words |
| `AppendAllMessageWordsToList(targetList)` | Appends all the message words from this message to the target list, and returns the count of words added. |
| `FillBuffer(byteOffset, targetList)` | Adds the message bytes to the buffer starting at the specified offset, and returns the count of bytes added. |

## IDL

[IMidiUniversalPacket IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/IMidiUniversalPacket.idl)

