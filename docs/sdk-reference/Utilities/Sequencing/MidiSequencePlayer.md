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

**Seeking sends the channel settings again.** Moving the position sends the bank, program, controllers, and pitch bend each channel should have at that point. So starting in the middle of a file doesn't play the rest of it with the wrong sound.

**Muting and soloing only hold back new notes.** Everything else is still sent. Otherwise, a track would come back with the wrong sound, and with a chord still sounding.

**`SilenceAllNotes` is called for you.** It runs when you stop, pause, or seek, and at the end of a sequence. It sends note offs for what's sounding, and then sustain off, all notes off, all sound off, and pitch bend center on each channel the sequence uses. Only call it yourself if you have some other reason to.

**Read `Position` on a timer, instead of waiting for an event.** A transport display updates tens of times a second, and an event for every frame would cost more than the drawing does.

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
