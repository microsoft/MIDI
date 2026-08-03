---
layout: sdk_reference_page
title: MidiSystemExclusiveSender
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
type: runtimeclass
description: Static class for sending System Exclusive data to an endpoint
---

This static class sends MIDI 1.0 System Exclusive (SysEx 7) data to an open endpoint connection. It reads the source data from a stream, converts it into SysEx 7 UMP messages, sends those messages to the destination endpoint and group, and reports progress along the way.

Sending a large SysEx dump as fast as possible can overwhelm the receiving device, so the send methods allow you to pace the transfer by pausing for a period of time after every *n* messages.

The methods return an `IAsyncOperationWithProgress` which reports progress using `MidiSystemExclusiveSendProgress`, and completes with `true` when the transfer finishes successfully. The operation is cancellable.

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `SendBinarySysEx7ByteDataAsync(destinationConnection, destinationGroup, dataSource, preferredSingleTransferMessageCount, transferSpacingMilliseconds, converterState)` | Reads MIDI 1.0 bytestream-format SysEx data from `dataSource`, converts it to SysEx 7 UMP messages, and sends it to the destination endpoint and group. |

### SendBinarySysEx7ByteDataAsync parameters

| Parameter | Description |
| --------------- | ----------- |
| `destinationConnection` | The `MidiEndpointConnection` to send the data to. This connection must already be open. |
| `destinationGroup` | The `MidiGroup` the messages will be addressed to. |
| `dataSource` | An `IInputStream` containing the raw MIDI 1.0 bytestream SysEx data, including the `0xF0` start and `0xF7` end bytes. This is typically a stream opened over a `.syx` file. |
| `preferredSingleTransferMessageCount` | The number of messages to send before pausing for `transferSpacingMilliseconds`. Set this, or `transferSpacingMilliseconds`, to zero to send without any pacing delay. |
| `transferSpacingMilliseconds` | How long to pause after each group of `preferredSingleTransferMessageCount` messages. Use this to give the receiving device time to process the data. |
| `converterState` | A `MidiBytestreamToUmpMessageConverterState` instance which holds the bytestream-to-UMP conversion state for this transfer. |

## Notes

The data in `dataSource` must be MIDI 1.0 bytestream System Exclusive data, not UMP data. Everything read from the stream is expected to convert into SysEx 7 (64-bit data) messages. If any other type of message is encountered in the data, the operation fails.

All arguments are required. The operation fails if any of them are null, or if `destinationConnection` is not open.

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
