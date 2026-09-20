---
layout: tools_page
title: MIDI File Player
tool: midiplayer
description: Play standard MIDI files to any MIDI device on your PC, and watch the notes as they play
icon: /assets/images/midiplayer.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

MIDI File Player plays standard MIDI files to any MIDI device on your PC: a hardware synth, a software instrument, a sound module, or the built-in General MIDI synthesizer. It shows you the music as it plays, so you can see which track is doing what rather than just hearing the result.

A MIDI file isn't audio. It's a list of instructions, and what it sounds like depends entirely on the instrument you send it to. That's the point of this app: the same file through your sound module and through the built-in synthesizer are two different performances, and you can switch between them while it plays.

![The MIDI File Player main window]({{ site.baseurl }}/assets/images/midiplayer.png)

## Getting started

1. Choose where the music should go from the **Play to** list at the bottom.
2. Select **Add files**, or drag MIDI files onto the window.
3. Select **Play**.

Files play one after another. Double-click a file in the queue to jump to it.

You can also open a MIDI file straight from File Explorer. MIDI File Player is added to the **Open with** menu for `.mid`, `.midi`, `.kar`, `.rmi`, and `.smf` files, and appears in **Settings &rsaquo; Apps &rsaquo; Default apps** if you want it to be the app that opens them. It deliberately doesn't take that over during installation &mdash; Windows doesn't allow an installer to claim a file type for you, and apps that try get reset.

Opening a file while the player is already running hands it to the window that's already open rather than starting a second one.

## Choosing where it plays

The **Play to** list at the bottom holds every MIDI output on the PC. Next to it, **Group** picks which group of that endpoint to play to, which matters for a MIDI 2.0 device with more than one. Most MIDI 1.0 gear has a single group and you can leave it alone.

Your choice is remembered, so the next time you open the player it's already pointing at the same instrument. If that instrument isn't connected when you start, the player tells you so rather than quietly playing to something else.

## Reading the display

The top of the window is the file that's playing.

| What you see | What it tells you |
|---|---|
| **File name** | The file currently playing |
| **Tracks and notes** | How many tracks the file has, and how many notes in total |
| **Lyrics** | The words at this point in the song. Only appears for files that have them |
| **TEMPO** | The tempo *right now*, not a single number for the file |
| **CHORD** | The chord in force right now. Only appears for files that have chord symbols in them |

**Tempo changes as the music plays, and that's not a bug.** A MIDI file doesn't have a tempo, it has a tempo map. The sample file that ships with Windows changes tempo twenty times. The number shown is the tempo at the playhead, so it moves when the music does.

**Chord symbols and lyrics are only there if the file has them.** Both panels stay out of the way completely on a file that doesn't, rather than showing you an empty label. Chord symbols in particular are uncommon &mdash; they turn up in lead sheet and karaoke files.

## The two views

There are two ways to watch the music, and the button at the right of the transport row switches between them.

The **track roll** is the default. Time runs left to right, each track has its own color, and the playhead sits a little in from the left so you can see what's coming. Notes that have already played are drawn dimmer, so the playhead reads as a boundary rather than a line you have to find.

The **keyboard view** shows notes falling towards a piano keyboard, and each key lights in the color of the track playing it. A note meets the key at the moment it sounds.

![Notes falling towards the keyboard]({{ site.baseurl }}/assets/images/midiplayer-keyboard-view.png)

Both views draw bar lines and beat lines from the file's own time signatures, and beats are dropped automatically when the music gets too dense for them to be useful.

Your choice is remembered between sessions.

## The track rail

Down the left of the note display is one row per track, and it's where most of the useful work happens.

| Part of the row | What it does |
|---|---|
| **Color bar** | Matches the color of that track's notes, and brightens as the track plays, so you can see at a glance which tracks are busy |
| **Name** | The track's name from the file, and the instrument it's set to |
| **M** | Mute the track |
| **S** | Play only this track |

**Tracks with no notes in them aren't listed.** A conductor track holding nothing but tempo changes has nothing to silence, so offering a mute button for it would be a lie.

The instrument name comes from the file itself where the file names it, and from the General MIDI instrument list where it doesn't.

**Muting only holds back the start of notes.** Notes already sounding still get their note off, and program changes and controllers still go out. If muting held everything back, unmuting would bring the track back playing the wrong sound, with a chord still hanging.

## The queue

The button at the right of the transport row shows and hides the queue.

Files play in order, and **Repeat** starts again from the top when the last one finishes. **Previous** restarts the file you're on if you're more than three seconds in, and goes back to the one before it if you're not, which is what a transport button usually does.

A file that couldn't be read stays in the list with a warning on it and says why &mdash; not a MIDI file, no longer there, couldn't be opened, or too large. That's more useful than silently dropping it, because the usual reason is that a file moved.

## Panic

Every MIDI file player has to deal with stuck notes, because MIDI notes are switched on and off separately and anything that interrupts the music between the two leaves a note sounding.

MIDI File Player sends note offs for everything that's sounding, then sustain off, all notes off, all sound off, and pitch bend back to center, **when you stop, pause, seek, mute, solo, load a new file, close the app, and at the end of a file**. That last one matters more than it sounds: real MIDI files are quite often missing their final note offs.

**It only does this on the channels the file actually uses.** Windows MIDI Services lets more than one application share a device, so blasting all sound off across all sixteen channels would silence whatever else is using the same instrument.

## Settings

The gear button at the top right opens the settings.

![The settings panel]({{ site.baseurl }}/assets/images/midiplayer-settings.png)

**Appearance** sets whether the app follows your Windows light or dark mode, and which window background to use. Mica and Acrylic pick up colors from your desktop; Acrylic is the one that lets what's behind the window show through.

The pin button next to the settings button keeps the player above your other windows.

## Starting from the command line

```
midiplayer.exe [files...] [options]

  [files...]             Optional. One or more MIDI files to add to the queue.

  --endpoint  id         Optional. The full endpoint device id to play to.

  --group     number     Optional. The group number, 1 through 16, to play to.

  --no-play              Optional. Add the files to the queue without starting
                         playback.
```

For example:

```
midiplayer.exe "C:\Windows\Media\onestop.mid" --group 1
```

You can find the endpoint device id for a device using the [MIDI Console]({{ site.baseurl }}/tools/console/) or the [MIDI Settings app]({{ site.baseurl }}/tools/settings/).

## Learn more

- [MIDI Patchbay]({{ site.baseurl }}/tools/midipatchbay/) to route the player's output through filters and transforms on its way to an instrument
- [MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/) to see exactly what the player is sending
- The sequencing and file reading used here are available to your own applications as [Windows.Devices.Midi2.Utilities.Sequencing]({{ site.baseurl }}/sdk-reference/Utilities/Sequencing/) and [Windows.Devices.Midi2.Utilities.Files]({{ site.baseurl }}/sdk-reference/Utilities/Files/)
