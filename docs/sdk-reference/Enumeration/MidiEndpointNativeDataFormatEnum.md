---
layout: sdk_reference_page
title: MidiEndpointNativeDataFormat
namespace: Windows.Devices.Midi2.Enumeration
type: enum
description: The data format that the device expects to send to and receive from Windows
---

The data format the device itself sends to and receives from Windows. It isn't always the format the driver gets from Windows.

| Scenario | Value |
| -------- | ----- |
| MIDI 2.0 device connected to the new UMP MIDI 2.0 driver | `UniversalMidiPacketFormat` |
| MIDI 2.0 device connected to the MIDI 1.0 class driver, using fallback mode | `Midi1ByteFormat` |
| MIDI 1.0 device connected to the new UMP MIDI 2.0 driver | `Midi1ByteFormat` |
| MIDI 1.0 device connected to a vendor driver | `Midi1ByteFormat` |
| MIDI 1.0 device connected to the MIDI 1.0 class driver | `Midi1ByteFormat` |

When you send messages through the WinRT API, you always use the Universal MIDI Packet (UMP) format, whatever the device's own format is. The MIDI service converts between the formats for you.

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `Unknown` | `0x00000000` | The format isn't known |
| `Midi1ByteFormat` | `0x00000001` | The device uses the MIDI 1.0 byte format |
| `UniversalMidiPacketFormat` | `0x00000002` | The device uses the Universal MIDI Packet format |
