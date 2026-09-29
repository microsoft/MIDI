---
layout: kb
title: Why a loopback mutes itself
audience: everyone
description: What loopback feedback protection does, how to find the loop it stopped, and when to turn it off
categories:
  - Troubleshooting
---

If a loopback suddenly stops passing MIDI, and MIDI Loopback Setup shows it as muted because of feedback, Windows MIDI Services found a feedback loop and muted the loopback to stop it. Nothing is broken. Find the loop, fix it, and then unmute the loopback.

This works the same way for both kinds of loopback: MIDI 1.0 Basic Loopbacks and MIDI 2.0 Loopbacks.

## What a feedback loop is

A loopback carries MIDI from one app to another. A feedback loop happens when MIDI that comes out of a loopback finds its way back into the same loopback. Every message comes back in, goes out again, comes back in again, and so on. It never stops by itself, and it doesn't slow down. It runs as fast as the PC can move it.

You'll notice it as stuck notes, controls that jump around, a DAW that lags or freezes, or a fan that spins up because the PC is suddenly busy.

A muted loopback stops the loop right away. The endpoints stay where they are, so no app loses its ports or needs setting up again.

## How to find the loop

- Look for MIDI thru, "echo MIDI input", "soft thru" and input monitoring settings in your DAW and other MIDI apps. Each of them sends what the app receives straight back out.
- Check routing and mapping apps. A route whose input and output are the same loopback is a loop.
- On a MIDI 2.0 loopback, A and B are the two ends of one cable. What goes into A comes out of B, and what goes into B comes out of A. So an app that passes MIDI through, listening to one end and sending to the other, makes a loop all by itself.
- If you can't tell which app is doing it, close your MIDI apps one at a time, unmuting the loopback after each one, until it stays unmuted.

To unmute, open [MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/) and select **Unmute** on the loopback's row. If it mutes itself again a moment later, the loop is still there.

## How it decides there's a loop

Busy MIDI on its own is never enough. Clock, controller lights, SysEx dumps and fast knob turns can all be busy, and that's fine.

When the traffic through a loopback looks like it's going around in a circle, Windows MIDI Services holds that loopback's messages for a fraction of a second. A loop only keeps going because the loopback keeps delivering it, so during the pause a real loop goes quiet. An app that's really sending keeps on sending. If the traffic keeps coming, it wasn't a loop, and the held messages are delivered right away, in order, with nothing lost.

It checks twice before it mutes anything.

## Apps that answer every message

An app that answers every message it receives by sending another one back, as fast as it can, is a loop on purpose. A tool that measures how fast MIDI makes a round trip is one example. Windows MIDI Services can't tell that apart from an accident, so it mutes the loopback.

To use an app like that, set its loopback to **Do nothing**. You can choose that when you create the loopback, or later by selecting **Edit** on the loopback's row in MIDI Loopback Setup. The setting is called **If MIDI feeds back into this loopback**.

**Do nothing** means that loopback isn't watched at all, so a real loop through it won't be stopped either. Use it only for the loopbacks that need it.

## Notifications

If the MIDI notifications app is running, you get a notification when a loopback mutes itself. Select it to open MIDI Loopback Setup.

To turn these notifications off, open MIDI Settings, select **Notifications**, and turn off **Loopbacks muted because of feedback**. Loopbacks still mute themselves when they find a loop. You just aren't told about it until you look.

## If the setting isn't there

Feedback protection needs a version of Windows MIDI Services that supports it. If MIDI Loopback Setup doesn't show **If MIDI feeds back into this loopback**, the version on this PC doesn't watch for feedback, and loopbacks never mute themselves. Muting a loopback by hand still works.

## For developers

- Check `MidiLoopbackManager.IsFeedbackProtectionAvailable` or `MidiBasicLoopbackManager.IsFeedbackProtectionAvailable` first. Each kind of loopback is checked on its own, because a PC can have one without the other.
- Set the choice for a new loopback with `FeedbackProtection` on `MidiLoopbackCreationConfig` or `MidiBasicLoopbackCreationConfig`, and change it later with `SetFeedbackProtection` on the manager. A change made with `SetFeedbackProtection` takes effect at once, and lasts until the MIDI service restarts. To keep it for a saved loopback, also save a `MidiLoopbackUpdateConfig` or `MidiBasicLoopbackUpdateConfig` with `FeedbackProtection` set, using `MidiServiceTransportPluginConfigManager.SaveUpdate`. MIDI Loopback Setup does this for you.
- `IsMutedForFeedback` and `FeedbackDetectedTime` on `MidiLoopbackEntry` and `MidiBasicLoopbackEntry` tell you whether a loopback muted itself, and when. Any change to the muted state clears them.
- If you collect a MIDI trace, the loopback transport logs a warning each time it mutes a loopback for feedback. It includes the loopback's association id, which direction the loop was in, and how fast messages were arriving.
