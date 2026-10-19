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

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in 2027. It's currently available for developers.

Windows MIDI Sequencer puts MIDI on a timeline, so you can build a song and play it back to your devices. Each track plays to one device. You can draw notes, set steps in a pattern, let a generator make a part for you, or record what you play. A clip launcher plays clips and scenes live, the way a groove box does.

It's made for MIDI 2.0. Notes keep their full detail, such as 16-bit velocity, and devices that speak MIDI 2.0 get MIDI 2.0. Devices that only speak MIDI 1.0 still play everything they can.

It doesn't record or play audio. To play sounds, it uses a synth or a device: the General MIDI Synth built into Windows, a hardware instrument, or a software instrument in another app.

## Quick start

![The Windows MIDI Sequencer main window with the sample sequence open, and numbered callouts on the File menu button, the transport buttons, the position and tempo display, the Launch, Snap, Capture and Silence controls, Undo and Redo, the track list, the clip launcher, the timeline, the editor, and the status bar]({{ site.baseurl }}/assets/images/midisequencer-quick-start.png)

1. **The File menu** has New sequence, Open, Open recent, Save, Import and Export, and a sample sequence to look around in. It also has **Lyrics…**, **About this sequence…**, **Studio setup…**, and **Remote control and MIDI Glass…**.
2. **The transport buttons** go to the start, play, stop, record, loop and play the metronome. Space plays and stops. To set where the loop goes, drag along the ruler above the timeline.
3. **The position and tempo display** shows where the playhead is, in bars and in time, and the tempo and meter. Select the tempo to change it. **Internal clock** opens **Clock and sync**, where the sequence can follow another device's clock or send MIDI Time Code.
4. **Launch** sets when a clip you launch starts playing, such as at the next bar. **Snap** sets the grid that clips and notes snap to. **Capture** makes a clip from what you just played on the armed tracks, even if you weren't recording. **Silence** stops every note on every device.
5. **Undo** and **Redo** go back and forward through your changes. The chip at the end says whether the sequence is saved.
6. **The tracks.** Each track plays to one device. Select a track to set it up. **M**, **S** and **R** mute it, solo it, and arm it to record. Folders hold tracks, and the **Tempo and meter** track at the top holds tempo changes and tags that mark parts of the song.
7. **The clip launcher** plays clips live. Each column is a scene. Select a clip to start it on its track, or a scene at the top to start every clip in its column at once. When there are more scenes than fit, scroll over the scene names, or hold Shift and scroll over the clips, to see the rest.
8. **The timeline** is the song from start to end. Each block is a clip. A clip can loop, and one clip can be placed in many spots, so a change to it changes every copy.
9. **The editor** shows the selected clip: a piano roll for notes, a grid of steps for a pattern, or the settings for a generator.
10. **The status bar** shows whether the MIDI service is running, which clock the sequence follows, and how many devices it plays to. Select the number of devices to open **Studio setup**.

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

Devices show the picture and description you gave them in MIDI Settings, in the device lists, on each track and in **Studio setup**. A device without a picture shows one for how it's connected, such as USB or Bluetooth. On a device with more than one port, such as a MIDI interface, a track shows the name of the port it plays to.

To see what Windows MIDI Services knows about a device, select **ⓘ** next to it in **Track setup** or in **Studio setup**. That shows its groups and whether each one speaks MIDI 2.0 or MIDI 1.0, how early the sequencer sends to it, how often automation goes to it, and what in the sequence uses it. To change a device's name, picture, description, offset or automation interval, use MIDI Settings.

Folders keep tracks together. Muting or soloing a folder mutes or solos everything in it. Right-click a track for more, such as **Pin to the top**, which keeps a track above everything that scrolls.

To move a track, drag it by its name. Drop it on a folder to put it inside, or between two rows to put it there. To work on several tracks at once, hold Ctrl and select each one, or hold Shift and select the last one to get every track in between. Drag any of them and they all move, in the same order.

### Choose each track's sound

Most synths have many sounds, such as a piano, strings or a drum kit. A track can pick one of them for you each time playback starts. Select the track's name to open **Track setup**, then select **Sound** under **Start-up messages**.

- Turn on **Choose this track's sound**. Then pick a sound from the list, or type its program number. Some devices also need a bank. The device's manual says which.
- Choosing a sound sends it to the device right away, so you can hear it. **Cancel** puts the old one back.
- The list shows the General MIDI names. The General MIDI Synth built into Windows uses them, and so do many other devices. A device that isn't General MIDI may have other sounds at those numbers. On channel 10, the list shows drum kits instead.
- A device that speaks MIDI-CI may share its own names for its sounds. To get them, select **Ask … for its sound names**. The sequencer asks only when you select that, and never asks a group whose **Play to** is off in **Studio setup**.

The track sends its sound as a program change, with its bank when you chose one, before anything else it sends when playback starts. **Start-up messages** lists anything else the track sends, in words.

### Make room for what you're working on

- **The track names.** Drag the edge between the track names and the timeline to make the names wider or narrower.
- **The launcher.** Drag the edge between the timeline and the launcher to give either one more room.
- **The editor.** Drag the top edge of the editor to make it taller or shorter.
- **One track.** Drag the bottom edge of a track to make it taller or shorter. **Tempo and meter** works the same way.
- **Compact tracks.** Right-click a track and select **Compact** to make it one line tall, so more tracks fit. If you selected several tracks, they all change. Select it again to put them back.

Double-click the edge beside the track names or the launcher, or the bottom edge of a track, to put it back the way it started. Track heights are saved with the sequence. The other sizes are saved with the app, so every sequence opens the same way.

### Move around and zoom

The bar under the timeline shows the whole sequence: a line for each track where its clips are, and the sections by name. The frame on it is what the timeline shows.

- **To move**, drag the frame, or select anywhere beside it to go there. Shift + mouse wheel scrolls through time too.
- **To zoom**, drag either end of the frame. You can also press Ctrl and turn the mouse wheel, or pinch on a touchpad or a touch screen, to zoom where you point. Dragging the frame up or down zooms too.
- **The keys at the bar's right end** zoom out, zoom in, and fit the whole sequence. On the timeline, Ctrl+plus and Ctrl+minus zoom, Ctrl+0 fits the sequence, and Ctrl+Shift+0 fits the selected clip.
- **Double-click the frame** to see the whole sequence, and again to go back.

The switch at the bar's left end changes it to a plain scroll bar. Right-click the bar to choose that, or to put it above the ruler instead. Both choices are also under **Appearance**.

### Choose a style

Select **Appearance** (the gear at the top right) and pick a style. Each one comes in light and dark, following the theme you choose there.

- **Night drive** is the Sequencer's own look. It sits on the window's backdrop: solid, Mica or acrylic.
- **Tactile** is one warm gray material, with keys and clips pressed out of it and soft shadows.
- **Aluminum** is machined metal plates under a dark strip, with graphite pads and backlit readouts.
- **Vellum** is paper and ink: hairlines instead of boxes, one vermilion accent, and clips as watercolor washes.

If you pick a window color of your own, Tactile, Aluminum and Vellum take it into the material itself, so they don't use the backdrop.

## Clips, patterns and generators

There are three kinds of clips:

- **Notes clips** hold notes you draw, type, record or import. The piano roll edits them. **B** turns on drawing, and **Q** moves notes onto the grid. Hold Shift while you drag notes to move them in time without changing their pitch.
- **Pattern clips** are step sequences, for drums or for melodies. Each lane has its own number of steps, so lanes can drift against each other and come back together. Steps can have a chance, or play only on some passes. **Swing** pushes every second step later. To change the order of the lanes, drag a lane by its name. **Make notes** turns a pattern into a notes clip that plays the same thing.
- **Generator clips** make their notes as they play. A **Euclidean rhythm** spreads hits as evenly as it can over a number of steps. A **Chance melody** picks notes from a scale. An **Arpeggiator** plays the notes you hold, or another track's notes, one after another. Eight more grow, learn, and spell their notes: **Life**, **Life drums**, **Learn**, **Words**, **Evolve**, the **Infinity series**, **L-system**, and **Change ringing**.

Right-click the timeline or an empty launcher slot to make any of them. The generators are under **New generator**.

To move a clip, drag it to another time, another track, or another launcher slot. Hold Ctrl as you let go to place it again instead of moving it. Both spots then play the same clip, so a change to one changes both. Drag a clip from the launcher onto the timeline, or from the timeline into a launcher slot, to place it there too. Hold Shift while you drag to keep the clip on its own track, so it moves only in time, or between that track's launcher slots.

### Start a pattern from a template

Not sure where to start? **Templates**, at the top of a pattern, has starter patterns, like the presets on a drum machine.

- **For a drum pattern**: drum machine classics (four on the floor, rock, disco, funk, shuffle, and breakbeat), electronic styles (house, techno, electro, UK garage, drum and bass, and dubstep), hip hop (boom bap, lo-fi, trap, and drill), and Latin, Caribbean, and African rhythms (reggaeton, Afrobeats, bossa nova, reggae one drop, and son clave).
- **For a melodic pattern**: basslines (acid techno, acid house, rolling techno, offbeat house, disco octaves, synthwave, funk, electro, trance gallop, UK garage, dub, trap 808, walking bass, and minimal techno) and sequences (a Berlin school sequence and an up and down arpeggio). Pick a key first, under **Key**. The sequencer remembers it until you close the window.

Point at a template to see the tempo the style is usually played at. Using a template doesn't change your sequence's tempo.

If the pattern already has steps, the sequencer asks what to do. **Replace** swaps them for the template. **Add on top** keeps your steps and adds the template's. Each drum goes on the lane that already plays it, and a drum you don't have gets a new lane. Where you both have a step, the template's step is used. Either way, **Undo** puts the pattern back the way it was.

The drum templates use the General MIDI drum notes. They sound right on the General MIDI Synth built into Windows, and on most drum machines, when the track plays on channel 10.

### Hear the steps

To hear each step as you add it, leave **Hear steps** (the speaker button at the top of a pattern) on. When you turn a step on, its sound plays, so you know which drum a lane plays before you start the sequence. In a melodic pattern, the up and down arrow keys play a step's new note too. Turn the button off to add steps quietly. The sequencer remembers your choice.

To change a number such as **Steps** or **Swing**, type it, or click in the box and then scroll or use the up and down arrow keys. With touch or a pen, up and down buttons appear beside the box when you tap it.

### Generators

A generator shows all of its settings in the editor. Select **Type** to change what kind of generator it is.

- **A Euclidean rhythm** draws its rings. Each dot is a step, and the lit dots are the hits. Drag around a ring to turn it, so its hits start later. Scroll over a ring to add or take away hits. The table next to it sets each ring's note, hits (**Pulses**), steps, turn (**Rotate**), velocity and chance. **What it plays** shows two bars of the result.
- **A chance melody** shows its first four passes, with new notes outlined, so you can see how much changes each time round.
- **An arpeggiator** sets where its notes come from, the order it plays them in, how fast it goes (**Rate**), and how many octaves it climbs.
- **Life** plays the Game of Life on a board with a column for each step and a row for each note of the key. Draw the first pass's cells on the board, or start with a scatter or a shape such as a glider. Each pass is the next generation: a cell with two or three neighbors lives on, and an empty cell with three is born. The edges wrap, so a glider that walks off the end of the bar comes back in at the start. Select any pass under the board to see it, and **Start from this pass** to begin there.
- **Life drums** does the same with a row for each drum. Select a drum's name on the board to change it.
- **Learn** learns which note tends to follow which (a Markov chain) from a tune, a clip you pick, or words you type, then makes up new melodies in that style, in the key you choose. **How closely it follows** sets how many notes back it remembers. The drawing shows the moves it learned, with this pass's lit. It learns only from what you give it.
- **Words** spells what you type as a melody. The letters climb the scale, a space rests, a period ends on the root, a question ends on the fifth, and capitals are louder. You hear the changes as you type.
- **Evolve** makes eight changed copies of each pass and plays the one that best fits the rules you weight, such as small steps or a motif that comes back. The drawing shows the copies and which one won, and the line under it shows the score climbing.
- **The infinity series** plays Per Nørgård's endless melody, which contains itself: every fourth note is the whole series again. Add voices to hear it against itself at half speed.
- **An L-system** grows a drawing from a short rule, like a plant, and the melody follows the drawing up and down. Pick a shape, or open **Rules** to write your own.
- **Change ringing** rings bells the way bell towers do, in a new order every row, with methods such as Plain Bob and Grandsire. The drawing is the method's blue line.

The melodies have a **Key and range**. Pick the key and the scale, and drag either end of the lit part of the keyboard to set the range, or drag its middle to move it. Scroll over the keyboard to move the range an octave. A chance melody and Learn can also keep to a **Largest jump**, and **Start and end on the root**.

Life, Learn, Words, Evolve, the infinity series, L-systems, and change ringing also have a **Velocity** section. Its chips pick how loud each note plays. Each generator's own ideas come first, such as **By age** for Life (new cells loudest), **By surprise** for Learn, **As spoken** for Words, and **Hand and back** for change ringing. **Steady**, **On the beat**, **Swell**, and **Random** work for all of them. After Learn learns from a clip, **As learned** plays the clip's loud and soft notes. The drawing shows this pass's notes over the range they play in. Drag the top or bottom edge of the band, or drag its middle to move it, or scroll over it to move it 10%.

Learn, Life, Life drums, Evolve, and change ringing can **Listen to** another clip or a whole track, even one that's a generator. Pick it from the list, and they learn from it straight away: Learn learns its melody, Life drops a pass of its notes onto the board as new cells, Evolve starts from it and gets a **Like what it hears** slider, and change ringing rings its most played notes as its bells. Turn on **Keep listening**, and each pass hears what the source played during the pass before, so it changes as the source does. A clip is heard as if both started together, and a track is heard where the listener plays. Two clips can even listen to each other, each a pass behind.

A generator plays the same notes on every pass, unless you set **Each pass** to **Changes**. The ones that grow, learn, and spell start out changing. The **Seed** is where its chances start. The same seed always plays the same way. You can type words as the seed, such as "Midnight drive," and the same words play the same way on any PC. To hear something different, select the button next to the seed to pick a new one.

## Recording

Select **R** on the tracks you want to record into, then select **Record**. A track records from the device set in its **Track setup**. To play into it from the screen, **Record from MIDI Keyboard** opens Windows MIDI Keyboard, already set up to play into the track. If the device the track plays to lists its sounds, MIDI Keyboard shows that list, so you can pick a sound by name.

If you played something good without recording, select **Capture**. It makes a clip from what you just played on the armed tracks.

## Automation and tempo

Automation lanes change a controller, pitch bend, pressure or program over time. Right-click a track and select **Add automation…**, then draw on the new lane. Drag a point to move it, and double-click a point to delete it. Hold Shift while you drag a point to keep its value, so it moves only in time.

A ramp goes to a device in small steps, no more often than the automation interval set for that device in MIDI Settings, or every 10 ms when it has none. That keeps a slow device, or an old MIDI cable, from falling behind. When a device has its own interval, its lanes say so.

The **Tempo and meter** track holds the tempo map. Click its tempo line to add a tempo change, and drag a change to move it or to make it faster or slower. Hold Shift while you drag to keep its tempo and move it only in time. Double-click a change to delete it. A tempo change can jump or ramp to the next one, and the meter can change at the start of any bar.

Tags mark parts of the song, like the verse and the chorus. They sit along the top of **Tempo and meter**. Double-click there to add one. Drag a tag to move it, and right-click it to rename it, change its color or delete it. To change where the loop starts and ends, drag its edges on the ruler, or drag the middle to move the whole loop.

## The MIDI Mixer

![The MIDI Mixer, with a strip for each track. Each strip has small knobs for expression, reverb and chorus, a larger pan knob, a tall volume fader with a scale and a meter beside it, M, S and W buttons, and the track's name on its color at the bottom]({{ site.baseurl }}/assets/images/midisequencer-mixer.png)

Select **MIDI Mixer** to show a strip for each track where the editor goes. It doesn't mix sound. It sends MIDI to each track's device, and the device changes its own volume and the rest. Each strip has the controls almost every synth responds to: volume, pan, expression, reverb and chorus. In MIDI, these are controllers 7, 10, 11, 91 and 93. You don't need to know those numbers to use them.

- **Moving a control** sends the change to the track's device right away, so you hear it. A device with an automation interval in MIDI Settings gets moves no more often than that, and always gets the last one.
- **The change is kept, too.** If the track has an automation lane for that control, the change sets the lane's point at the playhead. If it doesn't, it becomes the value the track sends when it starts playing.
- **While the sequence plays**, a lane keeps playing what it has. So a change to a control that has a lane is only heard, not kept, unless **W** is on.
- **To record your moves**, select **W** on a strip, then play the sequence and move the controls. Your moves are written into the track's automation lane for that control. If the track doesn't have one, it gets one.

A control shows where a lane has it at the playhead, so you can watch it move as the sequence plays. A faded control means the track hasn't set that value yet, so it shows where most synths start. **M** and **S** mute and solo the track, the same as in the track list.

To move a knob or the fader, drag it up or down, or use the mouse wheel. Hold Shift to move it in smaller steps. Double-click a control to put it back where most synths start. With the keyboard, press Tab to get to a control, then use the arrow keys.

- **Pan** says where the sound sits: **L** for left, **C** for center, or **R** for right, and how far. **L24** is 24 steps left of center. The pan knob lights up from the middle, and a screen reader says "left 24" or "center".
- **The fader's scale** runs from 0 to 127. The mark at 100 is where most synths start.
- **The meter** beside the fader shows how hard the track's notes are playing while the sequence plays. A note lights it from when the note starts until it's let go. It can't show how long the sound keeps ringing after that, because the device doesn't tell the sequencer.
- **The track's name**, at the bottom of its strip, is on the track's color. Select it to select the track. The selected track's strip has an outline.

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

## Studio setup

Some devices need more than notes. A drum machine or an arpeggiator can follow the sequence's MIDI clock, and some devices start and stop with it. **Studio setup** lists every device and group the sequence plays to or records from, so you can choose that after you've made your tracks, instead of before. Open it from the File menu, by selecting the number of devices in the status bar, or from **Clock and sync**.

For each device and group, choose whether the sequencer uses it. A group that only sends, such as a keyboard's, has only **Record from**. One that only receives, such as a synth's, has only **Play to**.

- **Play to** sends the group its notes and anything else it gets. Turn it off, and the sequencer sends nothing to that group, not even the clock. A device the sequencer uses for nothing else isn't even opened.
- **Record from** listens to what the group sends. Turn it off, and the sequencer ignores that group, including its clock.

Then choose what else each group it plays to gets:

- **Clock** sends MIDI clock, so the device keeps time with the sequence.
- **Start and stop** starts, stops and continues the device with the sequence.
- **Song position** tells the device where playback starts, so it starts in the same place.
- **Tempo** sends the tempo itself, as a MIDI 2.0 message, so the device knows a new tempo before the clock shows it. A group that speaks MIDI 1.0 can't take it, so its box is off.
- **Offset** sends the clock a little later or earlier, to line up a device that runs slow or quick.

To see what a group sends, select the eye on its row. That opens Windows MIDI Monitor on that device and group. MIDI Monitor only listens, so it sends the device nothing.

These choices belong to the PC, not the sequence, so every sequence that uses a device gives it the same. To send the clock to a device no track plays to, such as a drum machine that plays its own pattern, select **Add a device**. **Other MIDI devices on this PC** lists every other device, so you can look at what any of them is, or watch what it sends.

### A device that stops working when an app talks to it

Some devices, such as some Arduino projects, stop working when an app sends them a message they don't expect. Some apps send every device a question when they start, such as MIDI-CI Discovery or a MIDI Identity Request, to find out what it is.

The sequencer doesn't ask a device what it is on its own. It opens only the devices the sequence uses, and sends a device nothing until you play, try a sound or move a mixer control. It asks a device for its sound names over MIDI-CI only when you select **Ask … for its sound names** in a track's **Sound**. If a device still has trouble, turn off **Play to** for its group, and the sequencer leaves it alone. It won't even ask that group for its sound names.

Windows MIDI Keyboard is different. When it opens on a device, it asks the device for its sound names with MIDI-CI. The sequencer opens MIDI Keyboard from **Try this sound in MIDI Keyboard**, **Record from MIDI Keyboard** and the editor's **MIDI Keyboard** button. It won't open it on a group whose **Play to** is off, or ask that group for sound names.

## Clock, sync and remote control

**Clock and sync** opens from the clock button next to the tempo. It can follow another device's MIDI clock instead of the sequence's own tempo, and send MIDI Time Code for video and lighting gear. What each device gets of the sequence's own clock is chosen in **Studio setup**.

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

To open a sequence you've used before, select **File**, then **Open recent…**, or press Ctrl+Shift+O. It lists the last 100 sequences you opened or saved, newest first. Type in the box at the top to find one by its name or folder.

- Select the pin beside a sequence to keep it at the top of the list, so it never drops off. You can pin up to 100.
- **×** takes a sequence off the list. It doesn't delete the file.
- A sequence that was moved, renamed or deleted shows **Not found**. Open it from its new place with **Open…**, and it's back on the list.

The list and your pins are kept for your Windows account on this PC, not in the sequence files. Sharing a sequence doesn't share them.

## Not built yet

This preview doesn't have everything. These are known gaps:

- Automation lanes inside a clip play, but you can't draw them yet.
- Pattern steps can't change a controller or other settings yet, only notes.
- Lyrics don't have ruby (the small reading text used with Japanese), or two languages at once. Lyrics in an imported MIDI 2.0 clip file play, but they don't show in the Lyrics row.
- DAWproject export leaves out per-note expressions and polyphonic pressure.

## Learn more

- [Windows MIDI Glass]({{ site.baseurl }}/tools/midiglass/), to build a control surface for the sequencer
- [Windows MIDI Monitor]({{ site.baseurl }}/tools/midi2monitor/), to see what the sequencer sends
- [Windows MIDI Keyboard]({{ site.baseurl }}/tools/midikeyboard/), to play notes into a track from the screen
