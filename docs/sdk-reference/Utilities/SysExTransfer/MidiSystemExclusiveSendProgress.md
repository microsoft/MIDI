---
layout: sdk_reference_page
title: MidiSystemExclusiveSendProgress
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Progress information reported during a System Exclusive data transfer
---

This class carries the progress information reported by the asynchronous send operations on `MidiSystemExclusiveSender`. You receive an instance of it through the progress callback of the `IAsyncOperationWithProgress` returned by those methods.

Instances are created and updated by the sender. Applications do not create this type directly.

## Properties

| Property | Description |
| --------------- | ----------- |
| `CountBytesRead` | The total number of bytes read so far from the source data stream. |
| `CountMessagesSent` | The total number of UMP messages sent so far to the destination endpoint. |

## Notes

The source data is MIDI 1.0 bytestream-format SysEx, and each SysEx 7 UMP message carries up to six data bytes, so `CountBytesRead` typically advances much faster than `CountMessagesSent`. If you want to show a percentage-complete indicator, compare `CountBytesRead` against the known size of your source data rather than using the message count.
