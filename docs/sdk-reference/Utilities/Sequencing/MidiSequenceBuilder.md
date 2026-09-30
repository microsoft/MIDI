---
layout: sdk_reference_page
title: MidiSequenceBuilder
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Builds a sequence in memory, without a file
---

`MidiSequenceBuilder` makes a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) out of messages your app decides on, instead of out of a file on disk.

Use it when the music isn't in a file. A button that plays a short phrase, a pad that sends a patch change and then a few controllers, a System Exclusive dump with real gaps between its packets, or a test that needs sample data without writing a file: all of these need a sequence, and none of them start from a file.

What you get is the same `MidiSequence` a file reader makes. The player, the position maps, and a piano roll all work with it without knowing where it came from.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiSequenceBuilder()` | Creates an empty builder |

## Properties

| Property | Description |
| -------- | ----------- |
| `TicksPerQuarterNote` | How many ticks make a quarter note. Ignored when `TimingMode` is `Absolute`, because then a tick is a microsecond |
| `TimingMode` | [`MidiSequenceTimingMode`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimingModeEnum/). Musical ticks, or microseconds |

## Methods

| Method | Description |
| -------- | ----------- |
| `AddTrack(name)` | Adds a track and returns its index. Pass that index to the methods below. A sequence needs at least one track |
| `AddTempoChange(tick, beatsPerMinute)` | Sets the tempo from this tick on |
| `AddTimeSignature(tick, numerator, denominator)` | Sets the time signature from this tick on. The denominator must be a power of two |
| `AddNote(tick, durationTicks, trackIndex, channel, noteNumber, velocity)` | Adds a complete note, with its start and end |
| `AddMessages(trackIndex, tick, words)` | Adds one message as Universal MIDI Packet words |
| `AddSystemExclusive(trackIndex, tick, data)` | Adds a System Exclusive message, including its `F0` at the start and `F7` at the end |
| `GetSequence()` | Returns everything added so far, as a sequence ready to play |
| `Clear()` | Empties the builder so you can use it again |

## Remarks

**`AddNote` writes both halves of the note.** You give it a start and a length, and it adds the note on and the note off for you. A note that never ends is the most common mistake in a sequence built by hand. A builder that can't make one removes that problem completely.

**`AddMessages` is how you send what MIDI 1.0 can't.** The words go in and come out unchanged, so a 32-bit controller, a 16-bit velocity, or a per-note controller all come through. The one thing that does change is the group. It's set to the group the sequence is played on, because the group belongs to where a message is going, not to what was written.

**Bad input is skipped, not thrown as an exception.** A track index that doesn't exist, a note number above 127, a System Exclusive message missing its `F0`, or a tempo of zero is dropped, and the rest of the sequence is still built. To be sure everything you added made it in, check `EventCount` on the sequence you get back.

**`GetSequence` can be called more than once.** Each call gives you a new sequence and leaves the builder as it was, so you can add a few more messages and ask again.

## Example

A two note phrase on a synth:

```cpp
MidiSequenceBuilder builder{};

builder.TicksPerQuarterNote(480);
builder.AddTempoChange(0, 120.0);

auto const track = builder.AddTrack(L"Phrase");

builder.AddNote(0,   480, track, MidiChannel{ 0 }, 60, 100);
builder.AddNote(480, 480, track, MidiChannel{ 0 }, 64, 100);

MidiSequencePlayer player{ connection, MidiGroup{ 0 } };

co_await player.SetSequenceAsync(builder.GetSequence());

player.Play();
```

A pause that is a real pause, rather than a musical one:

```cpp
MidiSequenceBuilder builder{};

builder.TimingMode(MidiSequenceTimingMode::Absolute);

auto const track = builder.AddTrack(L"Dump");

// Ticks are microseconds here, so these packets are 20 milliseconds apart.
builder.AddSystemExclusive(track,     0, firstPacket);
builder.AddSystemExclusive(track, 20000, secondPacket);
builder.AddSystemExclusive(track, 40000, thirdPacket);
```

## Samples

These build two sequences and play them on the General MIDI synthesizer. The first counts in beats and slows down near the end. The second counts in microseconds, so its pauses last a set time whatever the tempo. Together they add notes, tempo changes, a time signature, a MIDI 2.0 controller, and System Exclusive.

* [C++/WinRT sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sequence-builder)
* [C# sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sequence-builder)

## See Also

- [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/)
- [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/)
- [`MidiSequenceTimingMode`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimingModeEnum/)
