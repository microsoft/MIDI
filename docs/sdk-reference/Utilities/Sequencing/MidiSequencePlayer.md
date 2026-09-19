---
layout: sdk_reference_page
title: MidiSequencePlayer
namespace: Windows.Devices.Midi2.Utilities.Sequencing
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Plays a sequence to one or more endpoints
---

`MidiSequencePlayer` plays a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/) to one or more endpoints.

The player hands its messages to the service with a timestamp for each and lets the service release them, rather than waking up to send each one itself. That is what keeps playback steady, and it is why a sequence has to be prepared before it starts.

## Getting a player

A player can borrow a connection your application already has open, or open and own one of its own. **Prefer borrowing.** A connection is not cheap, and an application which already has one should not be made to pay for a second.

| Constructor or method | Description |
| --------------------- | ----------- |
| `MidiSequencePlayer(connection, group)` | Borrows a [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection/) your application opened. Closing the player leaves it open |
| `CreateForEndpointAsync(session, endpointDeviceId, group)` | Opens and owns a connection on the supplied [`MidiSession`]({{ site.baseurl }}/sdk-reference/MidiSession/). Closing the player closes it. The session still belongs to you |

## Properties

| Property | Description |
| -------- | ----------- |
| `OwnsConnection` | True when the player opened the connection itself and will close it |
| `Connection` | The connection the player is sending to |
| `Sequence` | The sequence currently loaded |
| `State` | The current [`MidiSequencePlayerState`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerStateEnum/) |
| `Position` | A [`MidiSequencePlayerPosition`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayerPosition/) snapshot. Poll this to drive a display. There is deliberately no position event |
| `SoloTrackIndex` | The track to solo, or negative for none. Soloing silences the others without forgetting their own mute state |

## Methods

| Method | Description |
| ------ | ----------- |
| `SetSequenceAsync(sequence)` | Loads a sequence. Preparing converts it once, so this is the expensive call and `Play` is not |
| `Play()` | Starts or resumes playback |
| `Pause()` | Stops sending, keeping the current position |
| `Stop()` | Stops sending and returns to the start |
| `SeekToMicroseconds(microseconds)` | Moves to a point in time |
| `SeekToTick(tick)` | Moves to a tick |
| `GetTrackRouting(trackIndex)` | The [`MidiSequenceTrackRouting`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceTrackRouting/) for a track |
| `SetTrackRouting(trackIndex, routing)` | Sets where a track's messages go |
| `SetTrackMuted(trackIndex, muted)` | Mutes or unmutes a track |
| `IsTrackMuted(trackIndex)` | Whether a track is muted |
| `SilenceAllNotes()` | Silences everything this player has started |

## Events

| Event | Description |
| ----- | ----------- |
| `PlaybackEnded` | Raised when the sequence reaches its end |
| `StateChanged` | Raised when `State` changes |

## Remarks

**Seeking re-sends state.** Moving the position re-sends the bank, program, controllers and pitch bend each channel was left in, so starting in the middle of a file does not play the rest of it on the wrong sound.

**Muting and soloing only hold back note starts.** Everything else still goes out, because otherwise a track comes back on the wrong sound, and with a chord still sounding.

**`SilenceAllNotes` is sent for you.** It runs on stop, on pause, on seek and at the end of a sequence. It sends note offs for what is sounding, then sustain off, all notes off, all sound off and pitch bend center on each channel the sequence uses. Call it yourself only when you have some other reason to.

**Poll `Position` rather than asking for an event.** A transport display refreshes tens of times a second, and an event per frame would cost more than the drawing does.

**How far ahead messages are handed to the service is not settable**, and that is deliberate. The figure depends on how often the player sweeps and on how late a high resolution timer wakes, neither of which a caller can know, and the service refuses anything scheduled beyond five minutes ahead in any case.

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
