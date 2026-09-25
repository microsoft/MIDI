---
layout: sdk_reference_page
title: MidiStandardFileWriter
namespace: Windows.Devices.Midi2.Utilities.Files
type: runtimeclass
description: Writes a sequence out as a Standard MIDI File
---

`MidiStandardFileWriter` writes a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) out as a Standard MIDI File. It is the other half of [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/): read a file, write it back, and reading the result again gives you the same sequence.

The sequence can come from anywhere. A file you read, a sequence you built with [`MidiSequenceBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBuilder/), or a capture you recorded from a device are all the same to the writer.

This is a static class. There is nothing to construct.

Writing is asynchronous, and the whole file is built in memory before any of it is written. A Standard MIDI File declares each track's length in front of the track, so the length is not known until the track has been laid out, and half a file is worse than none at all.

## Static Methods

| Method | Description |
| ------ | ----------- |
| `WriteAsync(stream, sequence)` | Writes to an `IRandomAccessStream` with the default options |
| `WriteAsync(stream, sequence, options)` | Writes to an `IRandomAccessStream` using the supplied [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/) |
| `WriteToFileAsync(file, sequence)` | Writes to a `StorageFile` with the default options |
| `WriteToFileAsync(file, sequence, options)` | Writes to a `StorageFile` using the supplied [`MidiFileWriteOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteOptions/) |

All four return a [`MidiFileWriteResult`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileWriteResult/). Check `Succeeded` before telling anyone the file was saved.

## What "the same sequence" means

The same events at the same ticks, with the same tempo and meter maps, the same track names and the same text. It does not mean the same bytes.

A MIDI file is not a canonical form. The reader repairs things on the way in: it adds a tempo event when a file has none, it keeps only the last of two tempo events sharing a tick, and it decodes older text to UTF-8. Those repairs stay repaired. Write a file, read it back and write it again and you get the same bytes the second time, so nothing drifts as a file is opened and saved.

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
| System real time and system common | Escaped into the track the way the file format asks for |

Everything else is counted in `SkippedEventCount` and left out: per-note controllers, per-note pitch bend, per-note management, the relative controllers, 8-bit system exclusive, flex data, stream messages and the utility messages. None of them has a MIDI 1.0 form, so there is nothing to write.

## Two more things worth knowing

**Format 2 is written as format 1.** The reader lays the tracks of a format 2 file out one after another on a single timeline, so by the time a sequence exists the tracks are no longer independent and there is nothing left to write back.

**An absolutely timed sequence is laid out on a musical timeline.** A Standard MIDI File has no way of saying "a tick is a microsecond", so the writer puts the sequence at 120 beats per minute using `AbsoluteTimingTicksPerQuarterNote` from the options. Positions in real time are preserved; tick numbers are not.

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
