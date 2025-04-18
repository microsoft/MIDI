---
layout: sdk_reference_page
title: MidiEndpointNativeDataFormat
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: enum
description: The data format that the device expects to send to and receive from Windows
---

Represents the data format that the device expects to send to and to receive from Windows. This is not necessarily the same format as what the driver receives from Windows.

| Scenario | Value  |
| --------------- | ---------- | ----------- |
| MIDI 2.0 device connected to the new UMP MIDI 2.0 driver | `UniversalMidiPacketFormat` |
| MIDI 2.0 device connected to the MIDI 1.0 class driver, using fallback mode | `Midi1ByteFormat` |
| MIDI 1.0 device connected to the new UMP MIDI 2.0 driver | `Midi1ByteFormat` |
| MIDI 1.0 device connected to a vendor driver | `Midi1ByteFormat` |
| MIDI 1.0 device connected to the MIDI 1.0 class driver | `Midi1ByteFormat` |

Of course, when sending messages through the app SDK, you always use the Universal MIDI Packet (UMP) format. The MIDI Service will handle the translation between formats.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `Unknown` | `0` | Unknown native data format |
| `Midi1ByteFormat` | `1` | The native data format is the MIDI 1.0 byte message format |
| `UniversalMidiPacketFormat` | `2` | The native data format is the Universal MIDI Packet data format |
