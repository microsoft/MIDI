---
layout: sdk_reference_page
title: MidiBytestreamToUmpMessageConverterState
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Holds conversion state used when converting a stream of MIDI 1.0 bytes into UMP words
---

This class remembers where the converter is, when you turn a continuous stream of MIDI 1.0 bytes into Universal MIDI Packet (UMP) words over several calls. A MIDI 1.0 message can be split across two calls, and the bytes can use features like running status, so the converter needs a place to keep track of what it has seen so far.

Create one and pass it to the `MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords` overload that takes a converter state. Use the same state object for every call on the same stream, so partial messages and running status are handled correctly.

## Constructors

| Constructor | Description |
| --------------- | ----------- |
| `MidiBytestreamToUmpMessageConverterState()` | Creates a new, empty converter state |

## Properties

| Property | Description |
| --------------- | ----------- |
| `Tag` | An optional string your app can use to label this object, for example with the endpoint or group it's tracking |

## Samples

This object is what remembers that you're in the middle of a System Exclusive message between calls. Without it, the next buffer is read as if it were the start of a new message.

* [C++/WinRT sysex-send-bytes](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sysex-send-bytes)
* [C# sysex-send-bytes](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sysex-send-bytes)
* [C++/WinRT sysex-file-sender](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sysex-file-sender)
* [C# sysex-file-sender](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sysex-file-sender)
