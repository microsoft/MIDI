---
layout: sdk_namespace_page
title: WinRT API Sequencing and Playback
namespace: Windows.Devices.Midi2.Utilities.Sequencing
description: Music on a timeline, and a player which sends it to your MIDI endpoints
---

This namespace holds music on a timeline, called a sequence, and a player that sends it out. It isn't tied to any file format, on purpose. A Standard MIDI File read with [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/) gives you a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), and so does an app that builds one itself with [`MidiSequenceBuilder`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequenceBuilder/).

[`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/) gives its messages to the service with a timestamp on each one, and lets the service send them at the right time. It doesn't wake up to send each one itself. That's what keeps playback steady when the PC is busy, and it's why a sequence has to be prepared before playback starts.

## How the API is laid out, and why

A sequence isn't a collection of message objects. It's a way to reach the data it stores. Real files can have millions of events, so the busiest parts of a sequence are never handed to your app one item at a time:

* **Small** data that a display needs all of, such as the tempo map, time signatures, tracks, text, and lyrics, comes as normal `IVectorView` collections. Each is built once and kept.
* **Large** data, the notes, is copied into an array you own with `FillNotesInTickRange`. So drawing a frame takes one call, not one call per note.
* **The largest** data, the raw Universal MIDI Packets, is never handed to your app at all. The player reads it straight from the sequence.

If you're drawing a piano roll, this difference decides whether your app is fast or too slow to use.
