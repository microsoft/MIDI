# MIDI Message Types

Windows MIDI Services messages are all sent and received in Universal MIDI Packet (UMP) format. a UMP is made up of 1-4 32 bit MIDI words.

## Words

Several functions operate on 32 bit MIDI words directly. This is efficient for transmission, but may not be convenient for storage or processing. 

## Rich Types

The rich UMP types are full runtime classes, and so have more overhead than the fixed types or raw words. However, they offer conveniences not offered by the other types, including storage of the timestamp, message and packet type enumerations, and interface-based polymorphism. If your send/receive speed is not super critical, these are often the easiest solution.

If you are familiar with the `Windows.Devices.Midi` message types, these are the conceptual equivalent in UMP. 

For the most part, we do not provide strongly-typed discrete message types (like specific MIDI 2.0 Channel Voice messages or similar) in the API as that is a moving target, and many applications also include their own message creation and processing functions using their own libraries or any of the libraries included on [https://midi2.dev](https://midi2.dev). If there's demand for strongly-typed messages, we may provide them in the future.

### IMidiUniversalPacket

| Property | Description |
| -------- | ----------- |
| Timestamp | 64 bit timestamp set by the receiving transport in the case of incoming messages, or by the sender in the case of outgoing messages |
| MessageType | A [MidiMessageType enumeration value](./MidiMessageTypeEnum.md) which represents the 4 bit MIDI Message type 0x0 - 0xF as defined by the MIDI UMP standard. |
| PacketType | A [MidiPacketType enumeration value](./MidiPacketTypeEnum.md) which can be cast to an int to get the number of 32-bit words in the message packet |

| Function | Description |
| -------- | ----------- |
| PeekFirstWord() | Provides access to the first word of data, even if the message type and size is not yet known by the API user |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/IMidiUniversalPacket.idl)

### MidiMessage32

`MidiMessage32` is used for short messages such as utility messages and MIDI 1.0 messages in UMP format.

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| Word0 | First 32-bit MIDI word|

| Function | Description |
| -------- | ----------- |
| MidiMessage32() | Default constructor |
| MidiMessage32(timestamp, word0) | Construct a new message with a timestamp and 32 bit MIDI word |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessage32.idl)

### MidiMessage64

`MidiMessage64` is used for some data messages and for MIDI 2.0 Channel Voice messages.

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| Word0 | First 32-bit MIDI word|
| Word1 | Second 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| MidiMessage64() | Default constructor |
| MidiMessage64(timestamp, word0, word1) | Construct a new message with a timestamp and all 32 bit MIDI words |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessage64.idl)

### MidiMessage96

`MidiMessage96` is currently unused in the MIDI 2.0 UMP specification.

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| Word0 | First 32-bit MIDI word |
| Word1 | Second 32-bit MIDI word |
| Word2 | Third 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| MidiMessage96() | Default constructor |
| MidiMessage96(timestamp, word0, word1, word2) | Construct a new message with a timestamp and all 32 bit MIDI words |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessage96.idl)

### MidiMessage128

`MidiMessage128` is used for some data messages as well as important "Type F" stream metadata messages.

Includes all functions and properties in `IMidiUniversalPacket`, as well as:

| Property | Description |
| -------- | ----------- |
| Word0 | First 32-bit MIDI word |
| Word1 | Second 32-bit MIDI word |
| Word2 | Third 32-bit MIDI word |
| Word3 | Fourth 32-bit MIDI word |

| Function | Description |
| -------- | ----------- |
| MidiMessage128() | Default constructor |
| MidiMessage128(timestamp, word0, word1, word2, word3) | Construct a new message with a timestamp and all 32 bit MIDI words |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessage128.idl)

## Fixed Types

In addition to working with raw 32 bit words, the API provides a fixed structure type for messages.

### MidiMessageStruct

`MidiMessageStruct` is provided for cases where the API consumer wants to have a fixed value type they can use to send and receive messages. In the case of receiving messages, a function which fills the struct will typically return a count of valid words. The `MidiMessageStruct` struct type is simpler than the other runtime class types and may therefore perform better in some projections and for some uses. Note that this type does not include the timestamp field.

| Field | Description |
| -------- | ----------- |
| Word0 | First 32-bit MIDI word |
| Word1 | Second 32-bit MIDI word |
| Word2 | Third 32-bit MIDI word |
| Word3 | Fourth 32-bit MIDI word |

[IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiMessageStruct.idl)
