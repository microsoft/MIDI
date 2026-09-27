---
layout: sdk_reference_page
title: MidiMessageStruct
namespace: Windows.Devices.Midi2
type: struct
description: Represents a MIDI message in struct format
---

`MidiMessageStruct` is a plain value type with room for four 32-bit words, which is enough for any message. Use it when you want a fixed-size value for sending and receiving messages. When a function fills one in with an incoming message, it returns how many of the words are valid. Because it's simpler than the message classes, it can be faster in some languages. It doesn't hold a timestamp, so functions that use it take the timestamp separately.

## Struct Fields

| Field | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word |
| `Word1` | Second 32-bit MIDI word |
| `Word2` | Third 32-bit MIDI word |
| `Word3` | Fourth 32-bit MIDI word |
