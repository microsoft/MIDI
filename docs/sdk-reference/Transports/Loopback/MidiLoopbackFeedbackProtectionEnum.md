---
layout: sdk_reference_page
title: MidiLoopbackFeedbackProtection
namespace: Windows.Devices.Midi2.Transports.Loopback
type: enum
description: What a MIDI 2.0 loopback does when MIDI keeps coming back into it
---

A feedback loop happens when MIDI that comes out of a loopback finds its way back into the same loopback, so the same messages go around and around as fast as the PC can move them. This setting decides what the loopback does about it.

## Values

| Value | Numeric Value | Description |
| ----- | ------------- | ----------- |
| `Mute` | `0` | Watch for feedback, and mute the loopback pair when a loop is found, just as if it had been muted by hand. This is the default |
| `Off` | `1` | Don't watch for feedback at all |

## Why this is a setting

Muting is the right answer almost every time. A feedback loop can make stuck notes, freeze a DAW and keep the PC busy until someone notices.

`Off` is there for the rare app that answers every message it receives by sending another one back, as fast as it can. That really is a loop, just a deliberate one, and it can't be told apart from an accident.

Check `MidiLoopbackManager.IsFeedbackProtectionAvailable` before using this. When it's `false`, the loopback transport on this PC can't watch for feedback, and entries report `Off`.

Set it for a new loopback with `MidiLoopbackCreationConfig.FeedbackProtection`, change it with `MidiLoopbackManager.SetFeedbackProtection`, and read it from `MidiLoopbackEntry.FeedbackProtection`. See [Why a loopback mutes itself]({{ site.baseurl }}/kb/loopback-feedback-protection/) for how the loop is found.
