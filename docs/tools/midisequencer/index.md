---
layout: tools_page
title: Windows MIDI Sequencer
tool: midisequencer
description: Arrange, record and play MIDI 2.0 music on a timeline, with a clip launcher for playing live
icon: /assets/images/midisequencer.png
categories:
  - General Purpose MIDI Tools
---

> **This is a very early preview.** Windows MIDI Sequencer is new, and parts of it are still being built. Things will change, and some things don't work yet. Save often, and keep a copy of any sequence you care about. If something goes wrong, or you'd like something changed, tell us on [GitHub](https://github.com/microsoft/MIDI/issues).

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in November 2026. It's currently available for developers.

Windows MIDI Sequencer puts MIDI on a timeline, so you can build a song and play it back to your devices. Each track plays to one device. You can draw notes, set steps in a pattern, let a generator make a part for you, or record what you play. A clip launcher plays clips and scenes live, the way a groove box does.

It's made for MIDI 2.0. Notes keep their full detail, such as 16-bit velocity, and devices that speak MIDI 2.0 get MIDI 2.0. Devices that only speak MIDI 1.0 still play everything they can.

It doesn't record or play audio. To play sounds, it uses a synth or a device: the General MIDI Synth built into Windows, a hardware instrument, or a software instrument in another app.

## Quick start

![The Windows MIDI Sequencer main window with the sample sequence open, and numbered callouts on the File menu button, the transport buttons, the position and tempo display, the Launch, Snap, Capture and Silence controls, Undo and Redo, the track list, the clip launcher, the timeline, the editor, and the status bar]({{ site.baseurl }}/assets/images/midisequencer-quick-start.png)

1. **The File menu** has New sequence, Open, Save, Import and Export, and a sample sequence to look around in. It also has **Lyrics…**, **About this sequence…**, and **Remote control and MIDI Glass…**.
2. **The transport buttons** go to the start, play, stop, record, loop and play the metronome. Space plays and stops. To set where the loop goes, drag along the ruler above the timeline.
3. **The position and tempo display** shows where the playhead is, in bars and in time, and the tempo and meter. Select the tempo to change it. **Internal clock** opens **Clock and sync**, where the sequence can follow another device's clock or send its own clock to other devices.
4. **Launch** sets when a clip you launch starts playing, such as at the next bar. **Snap** sets the grid that clips and notes snap to. **Capture** makes a clip from what you just played on the armed tracks, even if you weren't recording. **Silence** stops every note on every device.
5. **Undo** and **Redo** go back and forward through your changes. The chip at the end says whether the sequence is saved.
6. **The tracks.** Each track plays to one device. Select a track to set it up. **M**, **S** and **R** mute it, solo it, and arm it to record. Folders hold tracks, and the **Tempo and meter** track at the top holds tempo changes and tags that mark parts of the song.
7. **The clip launcher** plays clips live. Each column is a scene. Select a clip to start it on its track, or a scene at the top to start every clip in its column at once.
8. **The timeline** is the song from start to end. Each block is a clip. A clip can loop, and one clip can be placed in many spots, so a change to it changes every copy.
9. **The editor** shows the selected clip: a piano roll for notes, a grid of steps for a pattern, or the settings for a generator.
10. **The status bar** shows whether the MIDI service is running, which clock the sequence follows, and how many devices it plays to.

### Make your first sequence

1. Open Windows MIDI Sequencer. A new sequence has one track that plays to the General MIDI Synth built into Windows.
2. Double-click the track's row on the timeline. A new clip appears there and opens in the editor.
3. Double-click in the piano roll to add a note. Drag a note to move it, or drag its end to make it longer.
4. Press Space to play, and Space again to stop.
5. Select **File**, then **Save**. Sequences are saved in **Documents &rsaquo; MIDI Sequences**.

To play a different device, select the track's name to open **Track setup**, and pick the device there. To play an app on the same PC, such as a DAW, pick **Default App Loopback (A)**, and set the app to listen to **Default App Loopback (B)**.

To look around first, select **File**, then **Open the sample sequence**.

## Tracks and devices

Each track plays to one device, on one group and channel. **Track setup** picks them. **As is** keeps each note on the channel it was recorded or drawn on. When a device has a timing offset set in MIDI Settings, the sequence sends to it a little early or late to line it up. You can turn that off in **Clock and sync**.

Folders keep tracks together. Muting or soloing a folder mutes or solos everything in it. Right-click a track for more, such as **Pin to the top**, which keeps a track above everything that scrolls.

## Clips, patterns and generators

There are three kinds of clips:

- **Notes clips** hold notes you draw, type, record or import. The piano roll edits them. **B** turns on drawing, and **Q** moves notes onto the grid.
- **Pattern clips** are step sequences, for drums or for melodies. Each lane has its own number of steps, so lanes can drift against each other and come back together. Steps can have a chance, or play only on some passes. **Swing** pushes every second step later. **Make notes** turns a pattern into a notes clip that plays the same thing.
- **Generator clips** make their notes as they play. A **Euclidean rhythm** spreads hits as evenly as it can over a number of steps. A **Chance melody** picks notes from a scale. A generator plays the same notes every time round, unless you turn on **Changes each pass**.

Right-click the timeline or an empty launcher slot to make any of them.

## Recording

Select **R** on the tracks you want to record into, then select **Record**. A track records from the device set in its **Track setup**. To play into it from the screen, **Record from MIDI Keyboard** opens Windows MIDI Keyboard, already set up to play into the track.

If you played something good without recording, select **Capture**. It makes a clip from what you just played on the armed tracks.

## Automation and tempo

Automation lanes change a controller, pitch bend, pressure or program over time. Right-click a track and select **Add automation…**, then draw on the new lane. The **Tempo and meter** track holds the tempo map. A tempo change can jump or ramp to the next one, and the meter can change at the start of any bar.

## Lyrics

![The Lyrics row under Tempo and meter, with numbered callouts on the Lyrics header, the lines of lyrics on the timeline, and the box near the bottom that shows the line being sung]({{ site.baseurl }}/assets/images/midisequencer-lyrics.png)

A sequence can have a lyrics track. Select **File**, then **Lyrics…**, and type the words.

1. **The Lyrics row** sits under **Tempo and meter**. It shows how many syllables there are, and which track sings them. Select the pencil to change the words.
2. **On the timeline**, each line starts where it's sung. Zoom in to see each syllable.
3. **While the sequence plays**, a box near the bottom shows the line being sung, with the words sung so far in color, and the next line under it.

When you type the words, put a hyphen between the syllables of a word, like `hap-py birth-day`. A space goes between words, each line of the song goes on its own line, and a blank line goes between verses. An underscore (`_`) holds a syllable over more than one note. Each syllable goes on the next note of the track that sings. If you change words that are already placed, the ones that were there keep their times.

Lyrics in a Standard MIDI File come in when you import it, including karaoke files (`.kar`). Exporting a Standard MIDI File writes them out again. While the sequence plays, the track that sings sends the words to its device as MIDI 2.0 lyric messages, if the device speaks MIDI 2.0. To use the lyrics in a music player or for subtitles, select **File**, then **Export**, then **Lyrics with times (LRC)…**.

## The clip launcher

The launcher plays clips without following the timeline. Select **Launcher** to show it. Select a clip to play it on its track, and select it again to stop it. A track playing a launched clip shows **Playing a launched clip** across its timeline. Select **Back to timeline** to go back to the timeline. **Launch** sets when a launched clip starts: now, or at the next sixteenth note, beat, bar, or more.

## Clock, sync and remote control

**Clock and sync** opens from the clock button next to the tempo. It can follow another device's MIDI clock, and it can send MIDI clock, song position and MIDI time code to other devices.

**Remote control and MIDI Glass…** on the File menu makes a MIDI endpoint for the sequencer, called **MIDI Sequencer**. A controller, another app, or a MIDI Glass layout can send to it to play, stop, launch clips and scenes, and mute, solo or arm tracks. The sequencer sends back what's playing, so a surface can light up. That dialog can also make a [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/) layout for the sequence.

## Importing and exporting

**File**, then **Import**, brings in:

- A **Standard MIDI File** (`.mid` or `.kar`), with each of its tracks as a track. Its lyrics and copyright notice come too.
- A **MIDI 2.0 clip file** (`.midi2`).
- A **DAWproject file** (`.dawproject`), which many DAWs can save. You pick a device for each of its instrument tracks.
- A **capture** from Windows MIDI Monitor or the `midi` console (`.csv`), with a track for each group.
- **Messages copied in Windows MIDI Monitor.** Ctrl+V pastes them as a clip on the selected track.

**File**, then **Export**, writes a Standard MIDI File, a MIDI 2.0 clip file for each track, a DAWproject file, or the lyrics as an LRC file. Each format has room for some things and not others. For example, DAWproject has no place for lyrics, and a Standard MIDI File carries MIDI 1.0. The sequence file always keeps everything.

## About this sequence

**File**, then **About this sequence…**, says who made the sequence and how. It's the same information Windows MIDI Glass and Windows MIDI Patchbay save with what you make: your name, your company or group, your website, and a license. It's what you say about your work. Windows doesn't check it.

**Copyright notice in exported files** is the notice that Standard MIDI Files, clip files and DAWproject files carry.

**How the sequence was made** starts with a suggestion based on the clips. A sequence of recorded clips suggests **Played and recorded**, and a mix of kinds suggests **A mix of things played, drawn and generated**. If any part was made with generative AI, choose an option that says so.

**Where the clips came from** lists every clip and how it began: recorded, drawn, generated, or imported. An imported clip keeps the name of its file and the file's copyright notice, which matters before you share or sell music that uses someone else's work.

## Your sequence files

Sequences are saved as `.midisequence` files in **Documents &rsaquo; MIDI Sequences**. A sequence that has been saved once saves itself again as you work. A file from a newer version of the app opens, but it can't be saved over, because it might hold settings this version doesn't know about. Use **Save as…** to keep a copy.

## Not built yet

This preview doesn't have everything. These are known gaps:

- The **arpeggiator** generator plays nothing yet. Its settings are kept.
- Automation lanes inside a clip play, but you can't draw them yet.
- Pattern steps can't change a controller or other settings yet, only notes.
- Lyrics don't have ruby (the small reading text used with Japanese), or two languages at once. Lyrics in an imported MIDI 2.0 clip file play, but they don't show in the Lyrics row.
- DAWproject export leaves out per-note expressions and polyphonic pressure.

## Learn more

- [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/), to build a control surface for the sequencer
- [Windows MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), to see what the sequencer sends
- [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/), to play notes into a track from the screen
