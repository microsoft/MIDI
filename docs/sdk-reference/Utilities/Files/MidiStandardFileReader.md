---
layout: sdk_reference_page
title: MidiStandardFileReader
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Reads a Standard MIDI File into a sequence you can play or draw
---

`MidiStandardFileReader` reads a Standard MIDI File, in any of its three formats, and the RIFF wrapper some files use around one. What comes back is a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/): music on a timeline, no longer tied to the file format it arrived in.

This is a static class. There is nothing to construct.

Reading is asynchronous because a file may come from anywhere, and because a large file is genuinely slow to parse. A file with hundreds of thousands of events is not unusual.

## Static Methods

| Method | Description |
| ------ | ----------- |
| `ReadAsync(stream)` | Reads from an `IRandomAccessStream` using the default limits |
| `ReadAsync(stream, options)` | Reads from an `IRandomAccessStream` using the supplied [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |
| `ReadFromFileAsync(file)` | Reads from a `StorageFile` using the default limits |
| `ReadFromFileAsync(file, options)` | Reads from a `StorageFile` using the supplied [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |

All four return a [`MidiFileReadResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadResult/). Check `Succeeded` before using `Sequence`.

## Remarks

A file which stops making sense partway through is read up to that point and reported as truncated, rather than being rejected, because a file with a damaged tail is usually still worth hearing. Set `FailOnUnreadableData` on the options if you would rather refuse it outright.

## Example

```cpp
auto result{ co_await MidiStandardFileReader::ReadFromFileAsync(file) };

if (result.Succeeded())
{
    auto sequence = result.Sequence();

    if (result.Truncated())
    {
        // still playable, but the file ended before it said it would
    }
}
```
