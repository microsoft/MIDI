---
layout: sdk_reference_page
title: MidiSystemExclusiveSendProgress
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Progress information reported during a System Exclusive data transfer
---

The progress reported while `MidiSystemExclusiveSender` sends data. You get one in the progress callback of the `IAsyncOperationWithProgress` that the send method returns.

The sender creates and updates these. Your app doesn't create them.

## Properties

| Property | Description |
| --------------- | ----------- |
| `CountBytesRead` | How many bytes have been read from the source data so far |
| `CountMessagesSent` | How many UMP messages have been sent to the destination endpoint so far |

## Notes

The source data is MIDI 1.0 bytestream SysEx, and each SysEx 7 UMP message carries up to six data bytes. So `CountBytesRead` usually goes up much faster than `CountMessagesSent`. To show a percent-complete bar, compare `CountBytesRead` to the size of your source data, instead of using the message count.
