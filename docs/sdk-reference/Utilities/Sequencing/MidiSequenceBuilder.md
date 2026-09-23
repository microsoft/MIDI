---
layout: sdk_reference_page
title: MidiSequenceBuilder
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
description: Builds a sequence in memory, without a file
---

`MidiSequenceBuilder` makes a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) out of messages your app decides on, instead of out of a file on disk.

Use it when the music is not in a file. A button that plays a short phrase, a pad that sends a patch change followed by a few controllers, a system exclusive dump with real gaps between its packets, a test that needs a fixture without writing one to disk: all of these need a sequence, and none of them start from a file.

What comes out is the same `MidiSequence` a file reader produces. The player, the position maps and a piano roll all work on it without knowing where it came from.

## Properties

| Property | Description |
| -------- | ----------- |
| `TicksPerQuarterNote` | How many ticks make a quarter note. Ignored when `TimingMode` is `Absolute`, because then a tick is a microsecond |
| `TimingMode` | [`MidiSequenceTimingMode`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimingModeEnum/). Musical ticks, or microseconds |

## Functions

| Function | Description |
| -------- | ----------- |
| `AddTrack(name)` | Adds a track and returns its index. Pass that index to everything below. A sequence needs at least one |
| `AddTempoChange(tick, beatsPerMinute)` | Sets the tempo from this tick onward |
| `AddTimeSignature(tick, numerator, denominator)` | Sets the time signature from this tick onward. The denominator must be a power of two |
| `AddNote(tick, durationTicks, trackIndex, channel, noteNumber, velocity)` | Adds a complete note |
| `AddMessages(trackIndex, tick, words)` | Adds one message as Universal MIDI Packet words |
| `AddSystemExclusive(trackIndex, tick, data)` | Adds a system exclusive message, including its leading `F0` and trailing `F7` |
| `GetSequence()` | Everything added so far, ready to play |
| `Clear()` | Empties the builder so it can be used again |

## Remarks

**`AddNote` writes both halves of the note.** You give it a start and a length, and it puts the note on and the note off in for you. A hand built sequence with a note that never ends is the most common way this goes wrong, and a builder that cannot express one removes the whole problem.

**`AddMessages` is how you send what MIDI 1.0 cannot say.** The words go in and come out untouched, so a 32 bit controller, a 16 bit velocity or a per note controller all survive. The one thing that does change is the group, which is set to the group the sequence is played on: the group belongs to where a message is going, not to what was written.

**Bad input is ignored rather than thrown.** A track index that does not exist, a note number above 127, a system exclusive message missing its `F0`, a tempo of zero: each of these is dropped and the rest of the sequence is still built. Check the `EventCount` on the sequence you get back if you want to be sure everything you added arrived.

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

## See Also

- [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/)
- [`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/)
- [`MidiSequenceTimingMode`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTimingModeEnum/)
