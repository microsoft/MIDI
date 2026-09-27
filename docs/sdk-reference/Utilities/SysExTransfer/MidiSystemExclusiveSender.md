---
layout: sdk_reference_page
title: MidiSystemExclusiveSender
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Static class for sending System Exclusive data to an endpoint
---

This static class sends MIDI 1.0 System Exclusive (SysEx 7) data to an open endpoint connection. It reads the source data from a stream, turns it into SysEx 7 UMP messages, sends those messages to the destination endpoint and group, and reports progress along the way.

Sending a large SysEx dump as fast as possible can overwhelm the receiving device. So the send method lets you slow the transfer down, by pausing for a while after every *n* messages.

The method returns an `IAsyncOperationWithProgress` that reports progress with `MidiSystemExclusiveSendProgress`, and finishes with `true` when the whole transfer works. You can cancel the operation.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `SendBinarySysEx7ByteDataAsync(destinationConnection, destinationGroup, dataSource, preferredSingleTransferMessageCount, transferSpacingMilliseconds, converterState)` | Reads MIDI 1.0 bytestream SysEx data from `dataSource`, turns it into SysEx 7 UMP messages, and sends them to the destination endpoint and group |

### SendBinarySysEx7ByteDataAsync parameters

| Parameter | Description |
| --------------- | ----------- |
| `destinationConnection` | The `MidiEndpointConnection` to send the data to. It must already be open |
| `destinationGroup` | The `MidiGroup` to send the messages to |
| `dataSource` | An `IInputStream` with the raw MIDI 1.0 bytestream SysEx data, including the `0xF0` start and `0xF7` end bytes. This is usually a stream opened on a `.syx` file |
| `preferredSingleTransferMessageCount` | How many messages to send before pausing for `transferSpacingMilliseconds`. Set this, or `transferSpacingMilliseconds`, to zero to send without pausing |
| `transferSpacingMilliseconds` | How long to pause after each batch of `preferredSingleTransferMessageCount` messages. Use it to give the receiving device time to handle the data |
| `converterState` | A `MidiBytestreamToUmpMessageConverterState` that holds the conversion state for this transfer |

## Notes

The data in `dataSource` must be MIDI 1.0 bytestream System Exclusive data, not UMP data. Everything in the stream must convert into SysEx 7 (64-bit data) messages. If the data holds any other kind of message, the operation fails.

All the arguments are required. The operation fails if any of them is null, or if `destinationConnection` isn't open.

## Example

```cs
var file = await StorageFile.GetFileFromPathAsync(sysExFilePath);

using (var stream = await file.OpenReadAsync())
{
    var converterState = new MidiBytestreamToUmpMessageConverterState();

    var operation = MidiSystemExclusiveSender.SendBinarySysEx7ByteDataAsync(
        connection,
        new MidiGroup(0),
        stream,
        100,        // send 100 messages at a time
        20,         // then wait 20ms before continuing
        converterState);

    operation.Progress = (op, progress) =>
    {
        Console.WriteLine($"Read {progress.CountBytesRead} bytes, sent {progress.CountMessagesSent} messages");
    };

    bool success = await operation;
}
```

## Samples

These samples send a `.syx` file to a device. They replace the WinMM steps of `midiOutPrepareHeader`, `midiOutLongMsg`, and `midiOutUnprepareHeader`, and the rules about how long each buffer has to stay around.

* [C++/WinRT sysex-file-sender](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sysex-file-sender)
* [C# sysex-file-sender](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sysex-file-sender)
