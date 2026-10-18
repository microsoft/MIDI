---
layout: sdk_reference_page
title: MidiSequencePlayer
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Plays a sequence to one or more endpoints
---

`MidiSequencePlayer` plays a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) to one or more endpoints.

The player gives its messages to the service with a timestamp on each one, and lets the service send them at the right time. It doesn't wake up to send each one itself. That's what keeps playback steady, and it's why a sequence has to be prepared before it starts.

## Getting a player

A player can borrow a connection your app already has open, or open one of its own. **Borrowing is better.** A connection costs memory and setup time, and an app that already has one shouldn't have to pay for a second.

| Constructor or method | Description |
| --------------------- | ----------- |
| `MidiSequencePlayer(connection, group)` | Borrows a [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) your app opened. Closing the player leaves it open |
| `CreateForEndpointAsync(session, endpointDeviceId, group)` | Opens a connection on your [`MidiSession`]({{ site.baseurl }}/sdk-reference/MidiSession/), and owns it. Closing the player closes the connection. The session is still yours |

## Properties

| Property | Description |
| -------- | ----------- |
| `OwnsConnection` | True when the player opened the connection itself, and will close it |
| `Connection` | The connection the player is sending to |
| `Sequence` | The sequence loaded now |
| `State` | The current [`MidiSequencePlayerState`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerStateEnum/) |
| `Position` | A [`MidiSequencePlayerPosition`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerPosition/) taken at the moment you read it. Read it from time to time to update a display. There's no position event, on purpose |
| `SoloTrackIndex` | The track to solo, or a negative number for none. Soloing silences the other tracks without changing whether each one is muted |
| `PlaybackRate` | How fast the sequence plays, as a multiple of the speed it was written at. `1.0` is as written, `0.5` is half speed and `2.0` is double. You can change it while the sequence plays. A value outside the minimum and maximum is clamped, and a value that isn't a number throws `E_INVALIDARG`. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `MinimumPlaybackRate` | The slowest `PlaybackRate` the player supports, which is `0.25`. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `MaximumPlaybackRate` | The fastest `PlaybackRate` the player supports, which is `2.0`. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `ChaseOptions` | What `Play` and a seek send before the music when they start partway into the sequence, as [`MidiSequenceChaseOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceChaseOptionsEnum/) flags. `ChannelSettings` by default. Add `HeldNotes` to restart the notes that should already be sounding. Bits the player doesn't know are ignored, and reading the property back shows what it will do. It stays set when you load another sequence. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `TransposeSemitones` | How many semitones every note is moved up, or down when it's negative. `0` plays the notes as written. Drums aren't moved. You can change it while the sequence plays. A value outside the minimum and maximum is clamped. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `MinimumTransposeSemitones` | The lowest `TransposeSemitones` the player supports, which is `-12`, an octave down. Added in version 2 of `MidiSequencingUtilityApiContract` |
| `MaximumTransposeSemitones` | The highest `TransposeSemitones` the player supports, which is `12`, an octave up. Added in version 2 of `MidiSequencingUtilityApiContract` |

## Methods

| Method | Description |
| ------ | ----------- |
| `SetSequenceAsync(sequence)` | Loads a sequence. It's converted once while loading, so this is the slow call, and `Play` is fast |
| `Play()` | Starts or resumes playback |
| `Pause()` | Stops sending, and keeps the current position |
| `Stop()` | Stops sending, and goes back to the start |
| `SeekToMicroseconds(microseconds)` | Moves to a point in time |
| `SeekToTick(tick)` | Moves to a tick |
| `GetTrackRouting(trackIndex)` | The [`MidiSequenceTrackRouting`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrackRouting/) for a track |
| `SetTrackRouting(trackIndex, routing)` | Sets where a track's messages go |
| `SetTrackMuted(trackIndex, muted)` | Mutes or unmutes a track |
| `IsTrackMuted(trackIndex)` | Whether a track is muted |
| `SilenceAllNotes()` | Silences every note this player has started |

## Events

| Event | Description |
| ----- | ----------- |
| `PlaybackEnded` | Raised when the sequence reaches its end |
| `StateChanged` | Raised when `State` changes |

## Remarks

**Starting partway in sends the channel settings again.** When `Play` starts partway into the sequence, or the position moves while playing, the player first sends the bank, program, controllers, and pitch bend each channel should have at that point. So starting in the middle of a file doesn't play the rest of it with the wrong sound. It only sends values the sequence set, and never resets a controller to zero. Set `ChaseOptions` to `None` to turn this off.

**Held notes come back only if you ask.** Add `HeldNotes` to `ChaseOptions`, and the player also restarts every note that should be sounding at that point, at the velocity it started with. Organ stops held on a control channel, drones, and pads need this. Leave it out for piano and drums, where a note restarted partway through gets a new attack. A note that ends exactly where playback starts isn't restarted, and neither is a note on a muted track. With `HeldNotes` on, muting or soloing a track while playing also restarts what the tracks you can still hear should be holding, because the silence a mute or solo sends turns off every held note. Unmuting a track restarts that track's held notes.

**A seek while playing waits for its own silence.** The silence sent on stop, pause, and seek goes out twice: once right away, and again about a third of a second later, after anything the service was already given. If playback starts again before that second pass lands, the music and the channel settings wait for it, so it can't cut off the new notes.

**Muting and soloing only hold back new notes.** Everything else is still sent. Otherwise, a track would come back with the wrong sound, and with a chord still sounding.

**`SilenceAllNotes` is called for you.** It runs when you stop, pause, or seek, and at the end of a sequence. It sends note offs for what's sounding, and then sustain off, all notes off, all sound off, and pitch bend center on each channel the sequence uses. Only call it yourself if you have some other reason to.

**Read `Position` on a timer, instead of waiting for an event.** A transport display updates tens of times a second, and an event for every frame would cost more than the drawing does.

**`PlaybackRate` changes the speed, not the music.** Every tempo in the sequence is scaled the same way, and the notes aren't changed, so the pitch stays the same. That makes it useful for learning a part slowly. A change made while playing is heard within about a quarter of a second, and nothing that's sounding is cut off. `Position` stays in the sequence's own time, so at half speed a two-minute sequence still reports a two-minute `DurationMicroseconds`, and `BeatsPerMinute` is still the tempo written in the file. Multiply it by `PlaybackRate` for the tempo you hear. The rate stays set when you load another sequence.

**`TransposeSemitones` changes the key, and leaves the drums alone.** On a drum kit, the note number picks which drum plays, so moving it would swap a snare for a tom. So notes on channel 10 never move. Neither do notes on any other channel the sequence has switched to a drum kit, with a bank select of 120 (General MIDI 2) or 127 (XG), or with a GS or XG message that makes it a rhythm part. A GM, GS or XG reset puts every channel back to normal. Poly pressure and the MIDI 2.0 per-note messages move with their note, and so does a MIDI 2.0 pitch attribute. A note moved below 0 or above 127 isn't played.

**A key change while playing never leaves a note hanging.** It's heard within about a quarter of a second. A note that's already sounding keeps its pitch until it ends, the way a keyboard's transpose button works, and its note off still reaches it. If a key is struck again before its first note ends, and the key changed in between, the player ends the first note before it starts the new one. The transposition stays set when you load another sequence.

**You can't set how far ahead messages are given to the service**, on purpose. The right amount depends on how often the player checks for messages to send, and on how late a high-resolution timer wakes up. Your app can't know either one. And the service refuses anything scheduled more than five minutes ahead anyway.

## Example

```cpp
// borrow a connection the app already has open
MidiSequencePlayer player{ connection, MidiGroup{ (uint8_t)0 } };

co_await player.SetSequenceAsync(sequence);

player.StateChanged([](auto&& sender, auto&&)
{
    // update the transport buttons
});

player.Play();
```

## Samples

* [C++/WinRT midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/midi-file-player) and [C# midi-file-player](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/midi-file-player) for pausing and resuming, reading `Position` to show the bar and beat, and handling `PlaybackEnded`
* [C++/WinRT sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/sequence-builder) and [C# sequence-builder](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/sequence-builder) for playing one sequence after another on the same player

Both borrow a connection the app opened, instead of letting the player open its own.
