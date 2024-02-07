---
layout: api_group_page
title: Messages
parent: Windows.Devices.Midi2 API
has_children: true
---

# Messages

Windows MIDI Services messages are all sent and received in Universal MIDI Packet (UMP) format, even if the device is a bytestream MIDI 1.0 device (we do the translation for you). a UMP is made up of 1-4 32 bit MIDI words, sized in 32 bit, 64 bit, 96 bit, and 128 bit packets. The first four bits of the packet are the message type, and from that, you can identify the type and size of message which follows.

## Words

Several functions operate on one or more 32 bit MIDI words directly. This is efficient for transmission, but may not be convenient for storage or processing. 

## Rich Types

The rich UMP types are full runtime classes, and so have more overhead than the fixed types or raw words. However, they offer conveniences not offered by the other types, including storage of the timestamp, message and packet type enumerations, and interface-based polymorphism. If your send/receive speed is not super critical, these are often the easiest solution.

If you are familiar with the `Windows.Devices.Midi` message types, these are the conceptual equivalent in UMP. 

For the most part, we do not provide strongly-typed discrete message types (like specific MIDI 2.0 Channel Voice messages or similar) in the API as that is a moving target, and many applications also include their own message creation and processing functions using their own libraries or any of the libraries included on [https://midi2.dev](https://midi2.dev). If there's demand for strongly-typed messages, we may provide them in the future.

## Fixed-Size Struct type

In addition to the richer types and raw words, the `MidiMessageStruct` type offers a fixed 128 bit message which can be used to send or receive any type of MIDI UMP.
