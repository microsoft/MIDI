---
layout: sdk_namespace_page
title: WinRT API Sequencing and Playback
namespace: Windows.Devices.Midi2.Utilities.Sequencing
description: Music on a timeline, and a player which sends it to your MIDI endpoints
---

This namespace holds a sequence of music on a timeline, and a player which sends it out. It is deliberately not tied to any file format: a Standard MIDI File read through [`MidiStandardFileReader`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileReader/) produces a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), and so does an application building one itself.

[`MidiSequencePlayer`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequencePlayer/) hands its messages to the service with a timestamp on each and lets the service release them, rather than waking up to send each one itself. That is what keeps playback steady under load, and it is why a sequence has to be prepared before playback starts.

## The shape of the API, and why

A sequence is a handle over its own storage rather than a collection of message objects. Files in the wild reach millions of events, so the dense parts of a sequence are never projected one item at a time:

* **Sparse** data a display genuinely needs in full — the tempo map, time signatures, tracks, text and lyrics — are ordinary `IVectorView` collections, built once and cached.
* **Dense** data — notes — is filled into an array you own, through `FillNotesInTickRange`, so drawing a frame costs one call rather than one call per note.
* **Densest** data — the raw Universal MIDI Packets — never crosses the boundary at all. The player reads the same storage directly.

If you are drawing a piano roll, this is the distinction which decides whether your app is fast or unusable.
