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

A MIDI file doesn't say how its text is encoded, so the reader works it out for each file. Text such as the title, the copyright, track names and lyrics comes back as UTF-8 in every case. The reader tries UTF-8 first, then Shift-JIS, which many older Japanese files use, and then Latin-1, which many older Western files use. It picks Shift-JIS only when every piece of text in the file reads cleanly that way and some of it is clearly Japanese, so Western text with accented letters stays as it was.

Some files do say how their text is encoded, using the tags from the MIDI Association's RP-026, and those tags win over the guess:

- A lyric that starts with `{@JP}` switches the lyrics that follow it to Shift-JIS, and `{@LATIN}` switches them to ANSI (Windows code page 1252), until the next tag. The tag is removed from the lyric, so it doesn't show up as a word to sing.
- The first of those tags in a file also decides the other text in the file, such as the title and track names, unless all of that text is UTF-8.
- After a tag RP-026 doesn't define, lyrics with characters outside ASCII are left out until a `{@JP}` or `{@LATIN}` tag appears, as RP-026 asks. Plain ASCII lyrics are kept.
- Any text that starts with a Unicode byte order mark is read as UTF-16 or UTF-8.

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
