---
layout: sdk_namespace_page
title: WinRT API Standard MIDI File Reading and Writing
namespace: Windows.Devices.Midi2.Utilities.Files
description: Read a Standard MIDI File into a playable sequence, and write one back out
---

This namespace reads MIDI files from disk or from a stream and produces a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/), which is no longer tied to any file format. Everything you do with the music after that, including playing it, lives in [`Windows.Devices.Midi2.Utilities.Sequencing`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/).

It also writes one back out. [`MidiStandardFileWriter`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileWriter/) takes any sequence, whether you read it from a file, built it yourself or recorded it from a device, and saves it as a Standard MIDI File. Read a file, write it back, and reading the result again gives you the same sequence.

Reading and writing are both asynchronous, because a MIDI file may come from anywhere: a download, a mail attachment, or a document a browser handed to your app.

A MIDI file usually arrives from somewhere your application does not control, so nothing is allocated from a length the file declares without first checking that the bytes are really there. [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) is where you set those ceilings if the generous defaults are not what you want.
