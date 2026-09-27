---
layout: sdk_namespace_page
title: WinRT API Standard MIDI File Reading and Writing
namespace: Windows.Devices.Midi2.Utilities.Files
description: Read a Standard MIDI File into a playable sequence, and write one back out
---

This namespace reads MIDI files from disk or from a stream, and turns them into a [`MidiSequence`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/MidiSequence/). A sequence isn't tied to any file format. Everything you do with the music after that, including playing it, is in [`Windows.Devices.Midi2.Utilities.Sequencing`]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/).

It also writes one back out. [`MidiStandardFileWriter`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiStandardFileWriter/) takes any sequence, whether you read it from a file, built it yourself, or recorded it from a device, and saves it as a Standard MIDI File. Read a file, write it back, and reading the result again gives you the same sequence.

Reading and writing are both asynchronous, because a MIDI file may come from anywhere: a download, an email attachment, or a document a browser handed to your app.

A MIDI file usually comes from somewhere your app doesn't control. So the reader never sets aside memory for a length the file claims, until it has checked that the bytes are really there. [`MidiFileReadOptions`]({{ site.baseurl }}/sdk-reference/Utilities/Files/MidiFileReadOptions/) is where you set those limits, if the generous defaults aren't what you want.
