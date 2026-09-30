---
layout: sdk_reference_page
title: MidiStandardFileReader
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Reads a Standard MIDI File into a sequence you can play or draw
---

`MidiStandardFileReader` reads a Standard MIDI File in any of its three formats, including files wrapped in the RIFF format some files use. What you get back is a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/): music on a timeline, no longer tied to the file format it came in.

This is a static class. There's nothing to create.

Reading is asynchronous because a file may come from anywhere, and because a large file takes a while to read. A file with hundreds of thousands of events isn't unusual.

## Static Methods

| Method | Description |
| ------ | ----------- |
| `ReadAsync(stream)` | Reads from an `IRandomAccessStream` with the default limits |
| `ReadAsync(stream, options)` | Reads from an `IRandomAccessStream` with these [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |
| `ReadFromFileAsync(file)` | Reads from a `StorageFile` with the default limits |
| `ReadFromFileAsync(file, options)` | Reads from a `StorageFile` with these [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) |

All four return a [`MidiFileReadResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadResult/). Check `Succeeded` before using `Sequence`.

## Remarks

A file that stops making sense partway through is read up to that point and reported as truncated, instead of being rejected, because a file with a damaged end is usually still worth hearing. Set `FailOnUnreadableData` on the options if you'd rather refuse it completely.

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

## Samples

These read a file, show its title, length, tempo, time signature, and tracks, and then play it. Pass the path of a MIDI file on the command line, or leave it off to play one that comes with Windows.

* [C++/WinRT midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/midi-file-player)
* [C# midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/midi-file-player)
