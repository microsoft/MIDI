---
layout: sdk_reference_page
title: MidiStandardFileWriter
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Writes a sequence out as a Standard MIDI File
---

`MidiStandardFileWriter` writes a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) out as a Standard MIDI File. It's the other half of [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/). Read a file, write it back, and reading the result again gives you the same sequence.

The sequence can come from anywhere. A file you read, a sequence you built with [`MidiSequenceBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBuilder/), or a recording from a device are all the same to the writer.

This is a static class. There's nothing to create.

Writing is asynchronous, and the whole file is built in memory before any of it is written. A Standard MIDI File puts each track's length in front of the track, so the length isn't known until the track has been laid out. And half a file is worse than no file at all.

## Static Methods

| Method | Description |
| ------ | ----------- |
| `WriteAsync(stream, sequence)` | Writes to an `IRandomAccessStream` with the default options |
| `WriteAsync(stream, sequence, options)` | Writes to an `IRandomAccessStream` with these [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/) |
| `WriteToFileAsync(file, sequence)` | Writes to a `StorageFile` with the default options |
| `WriteToFileAsync(file, sequence, options)` | Writes to a `StorageFile` with these [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/) |

All four return a [`MidiFileWriteResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteResult/). Check `Succeeded` before telling anyone the file was saved.

## What "the same sequence" means

The same events at the same ticks, with the same tempo and meter maps, the same track names, and the same text. It doesn't mean the same bytes.

There's more than one way to write the same music in a MIDI file. The reader fixes some things as it reads: it adds a tempo event when a file has none, it keeps only the last of two tempo events at the same tick, and it converts older text to UTF-8. Those fixes stay fixed. Write a file, read it back, and write it again, and you get the same bytes the second time. So nothing slowly changes as a file is opened and saved over and over.

## What MIDI 1.0 cannot hold

A Standard MIDI File is a MIDI 1.0 format. A sequence built from Universal MIDI Packets is translated where MIDI 1.0 has an equivalent:

| Universal MIDI Packet | Written as |
| --------------------- | ---------- |
| MIDI 1.0 channel voice | The same message |
| MIDI 2.0 note on and note off | A MIDI 1.0 note, velocity scaled to 7 bits. A note on that would scale to zero is written at velocity 1, so it does not turn into a note off |
| MIDI 2.0 control change, poly pressure, channel pressure | The same message, value scaled to 7 bits |
| MIDI 2.0 pitch bend | A 14-bit pitch bend |
| MIDI 2.0 program change | A program change, with bank select in front of it when the message carries a bank |
| Registered and assignable controllers | The four control change messages MIDI 1.0 uses for RPN and NRPN |
| 7-bit system exclusive | Reassembled into one complete dump, however many packets it arrived in |
| System real time and system common | Stored in the track with the escape the file format provides |

Everything else is counted in `SkippedEventCount` and left out: per-note controllers, per-note pitch bend, per-note management, the relative controllers, 8-bit System Exclusive, flex data, stream messages, and the utility messages. None of them has a MIDI 1.0 form, so there's nothing to write.

## Two more things to know

**Format 2 is written as format 1.** The reader puts the tracks of a format 2 file one after another on a single timeline. So by the time a sequence exists, the tracks aren't separate anymore, and there's no format 2 structure left to write back.

**A sequence with absolute timing is laid out on a musical timeline.** A Standard MIDI File has no way to say "a tick is a microsecond," so the writer puts the sequence at 120 beats per minute, using `AbsoluteTimingTicksPerQuarterNote` from the options. The real-time position of each event is kept. The tick numbers aren't.

## Example

```cpp
MidiFileWriteOptions options{};

options.UseRunningStatus(true);

auto result{ co_await MidiStandardFileWriter::WriteToFileAsync(file, sequence, options) };

if (result.Succeeded())
{
    if (result.SkippedEventCount() > 0)
    {
        // the sequence held messages MIDI 1.0 cannot express
    }
}
```

## Samples

These build a short sequence, save it in your temporary folder, and read it back to compare. The sequence includes a MIDI 2.0 controller, which is written with its value scaled to 7 bits, and two per-note pitch bends, which MIDI 1.0 can't hold and `SkippedEventCount` counts. They also write the same sequence as a single track, to memory instead of to a file.

* [C++/WinRT midi-file-writer](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/midi-file-writer)
* [C# midi-file-writer](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/midi-file-writer)
