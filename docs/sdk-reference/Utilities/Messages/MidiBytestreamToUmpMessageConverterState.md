---
layout: sdk_reference_page
title: MidiBytestreamToUmpMessageConverterState
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Holds conversion state used when converting a stream of MIDI 1.0 bytes into UMP words
---

This class holds the state required when converting a continuous stream of MIDI 1.0 bytestream data into Universal MIDI Packet (UMP) words across multiple calls. Because MIDI 1.0 bytestream data can span message boundaries, and can use features like running status, the converter needs a place to keep track of what it has seen so far.

Create an instance of this class and pass it to the `MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords` overload which accepts a converter state. Reuse the same state instance across calls for a given stream so that partial messages and running status are handled correctly.

## Constructors

| Constructor | Description |
| --------------- | ----------- |
| `MidiBytestreamToUmpMessageConverterState ()` | Creates a new, empty converter state instance. |

## Properties

| Property | Description |
| --------------- | ----------- |
| `Tag` | An optional application-defined string you may use to identify or annotate this state instance (for example, the source endpoint or group it is tracking). |
