---
layout: sdk_reference_page
title: MidiMessageStruct
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Represents a MIDI message in struct format
---

`MidiMessageStruct` is provided for cases where the API consumer wants to have a fixed value type they can use to send and receive messages. In the case of receiving messages, a function which fills the struct will typically return a count of valid words. The `MidiMessageStruct` struct type is simpler than the other runtime class types and may therefore perform better in some projections and for some uses. Note that this type does not include the timestamp field.

## Struct Fields

| Field | Description |
| -------- | ----------- |
| `Word0` | First 32-bit MIDI word |
| `Word1` | Second 32-bit MIDI word |
| `Word2` | Third 32-bit MIDI word |
| `Word3` | Fourth 32-bit MIDI word |
