---
layout: sdk_namespace_page
title: WinRT API Standard MIDI File Reading
namespace: Windows.Devices.Midi2.Utilities.Files
description: Read a Standard MIDI File into a playable sequence
---

This namespace reads MIDI files from disk or from a stream and produces a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), which is no longer tied to any file format. Everything you do with the music after that, including playing it, lives in [`Windows.Devices.Midi2.Utilities.Sequencing`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/).

Reading is asynchronous, because a MIDI file may come from anywhere: a download, a mail attachment, or a document a browser handed to your app.

A MIDI file usually arrives from somewhere your application does not control, so nothing is allocated from a length the file declares without first checking that the bytes are really there. [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) is where you set those ceilings if the generous defaults are not what you want.
