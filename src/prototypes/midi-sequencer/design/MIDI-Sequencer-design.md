# Windows MIDI Sequencer: design

This is the design for **Windows MIDI Sequencer**, a new app in the Windows MIDI Services tool family. It records and plays MIDI, MIDI 2.0 first, and it combines clip launching, step patterns, generators and an ordinary timeline in one window. The comps are the HTML pages in this folder (pictures in `shots/`). Your answers to the first round are in section 22, and the phases table in section 20 shows how far building has got.

It covers what the app is and isn't, how a sequence is organized, where step sequencers and generators belong, how MIDI 2.0 shapes the design, recording, automation, clock and timing, the metronome, files, import and export (including DAWproject), provenance, what we can reuse (including the Sequencing API question), how it fits with the other tools (including MIDI Keyboard and MIDI Glass), phases, risks, and decisions. It doesn't cover code structure below the level of "which engine owns what". That comes with phase 1.

## 1. What it is, and what it isn't

- **It records and plays MIDI. That's all it does with sound.** No audio engine, no WASAPI or ASIO, no plug-ins, no mixer. The sound comes from your devices, from the General MIDI Synth in Windows MIDI Services, or from another app reached through a loopback.
- **MIDI 2.0 first.** Clips hold MIDI 2.0 data at full resolution: 16-bit velocity, 32-bit controllers, per-note pitch bend and per-note controllers, note attributes such as exact pitch. MIDI 1.0 devices get MIDI 1.0, and the app says ahead of time what they'll miss (section 5).
- **One sequence per window.** A sequence is the document, saved as a `.midisequence` file. Tracks run down the left. Each track has a row of clip slots (the clip launcher) and a timeline, side by side, sharing the same row.
- **Three kinds of clip:** notes (recorded or drawn), pattern (a step sequencer) and generator (notes worked out as it plays, from settings and a seed). All three launch, loop, link and sit on the timeline the same way.
- **Any number of tracks and clips, within reason.** Limits exist only to protect against a damaged or hostile file, and they're set far above what a person builds. The real numbers come from measuring in phase 0, not from a guess.

It isn't a notation editor, a DAW, a router (MIDI Patchbay does that) or a control surface (MIDI Glass does that). If we build something with audio later, it should open these sequences. That's one of the reasons the file format is plain and documented (section 9).

## 2. The comps

| Comp | What it shows |
| --- | --- |
| [1-main.html](1-main.html) | The main window: pinned rows, folders, the launcher beside the timeline, a track playing a launched clip, linked clips, automation drawn on a track, tags on the Tempo and meter track and on a track, recording into a slot, the metronome and MIDI Keyboard buttons, the clip editor at the bottom. |
| [1-main.html?theme=light](1-main.html?theme=light) | The same window in the light theme (picture: `shots/1-main-light.png`). |
| [2-piano-roll.html](2-piano-roll.html) | A notes clip in its own window, playing to a MIDI 2.0 destination: per-note pitch bend drawn on the notes, exact pitch, 16-bit velocity, per-note pan, ghost notes, a scale, per-note chance. |
| [3-pattern-clip.html](3-pattern-clip.html) | Pattern clips: lanes with their own length, per-step chance, ratchets, conditions, nudges and locks, and a melodic mode with accent and slide. |
| [4-generator-clips.html](4-generator-clips.html) | Generator clips: Euclidean rhythm, a chance melody that drifts from pass to pass, an arpeggiator. |
| [5-track-setup.html](5-track-setup.html) | A track's source and destination, echo while armed, recording from MIDI Keyboard, MIDI 2.0 or MIDI 1.0 per destination, each device's offset from MIDI Settings, start-up messages, a missing device. The launcher is hidden here. |
| [6-clock-sync.html](6-clock-sync.html) | Internal tempo or another device's clock, sending clock to several devices with offsets, MIDI 2.0 tempo messages, MIDI Time Code out, and the metronome. |
| [7-import-export.html](7-import-export.html) | Importing a Standard MIDI File, a DAWproject file and a MIDI Monitor capture, pasting from MIDI Monitor, exporting MIDI 2.0 clip files, a Standard MIDI File or DAWproject. |
| [8-provenance.html](8-provenance.html) | About this sequence: the same provenance block as MIDI Glass and MIDI Patchbay, plus where each clip came from. |

**Light and dark.** The other comps are drawn in the dark theme. The app follows the rest of the family: light and dark themes (or the Windows setting), and the background color and backdrop from the appearance flyout, like MIDI Glass and MIDI Patchbay. When the app is built, I'll match the comps as closely as WinUI allows and call out anywhere it can't.

## 3. How a sequence is organized

**A sequence** has a Tempo and meter track, any number of tracks and folders, a set of scenes (the launcher's columns), the clips those refer to, and tags.

**A track** has a name, a color, a source (where it records from), a destination (where it plays to), M, S and R buttons (mute, solo, record arm), automation lanes and a short list of start-up messages. One destination per track keeps the model simple. To play the same part on two synths, duplicate the track: the clips stay linked, so an edit reaches both.

**Folders** hold tracks and other folders. M and S on a folder apply to everything in it. A closed folder draws an outline of what its tracks play, so the song's shape is still visible.

**Pinned rows** stay at the top of the track list, the launcher and the timeline while everything else scrolls, like frozen rows in Excel. Any track or folder can be pinned. The Tempo and meter track is pinned by default. A pinned track still belongs to its folder (the folder's M and S still apply). It's just drawn in the pinned area, with the folder's name in front of it.

**Clips.** A clip is notes, a pattern or a generator, with a length and a loop. Clips sit in launcher slots and on the timeline, and **a clip can appear in many places: it's the same clip each time.** Edit it once and every placement changes. Make a copy to break the link. The timeline shows a link mark and a count. I chose linked by default because it's what makes patterns and loops practical: a beat used forty times is one beat, not forty.

**Launcher or timeline, per track.** A track follows the timeline until you launch a clip on it. Then it plays that clip until you stop it or choose Back to timeline (per track, or for every track from the transport). This is how Bitwig Studio and Ableton Live handle the same choice, and it means the launcher and the timeline never fight over a track. Launching is quantized (next bar by default), so a clip starts in time.

**Scenes** are the launcher's columns. Launching a scene launches every clip in it. An empty slot stops its track when the scene launches, unless you take the stop button out of that slot.

**The launcher can be hidden.** Then the timeline gets the width (comp 5). Someone who only arranges never has to see it.

**Tags mark moments.** A tag is a short piece of text at a moment, drawn as a label on the timeline (comp 1). A tag on the Tempo and meter track is about the whole sequence, like "Chorus" or "Key change". A tag on a track is about that track, like "Filter opens here" or "Retake this". Tags can have a color, can be dragged, and are listed in a Go to tag menu, so they double as bookmarks. They're kept in the file and go out as markers when you export (sections 11 and 12). A tag never sends anything to a device.

## 4. Step sequencers and generators: my recommendation

You asked whether step sequencing, probability and algorithmic sequencers should be the source of looping clips, or a separate app. **My recommendation is clips, in this app: a pattern clip for classic step sequencing, and a generator clip for algorithmic parts.** Here's why.

1. **They need everything a clip already has.** A transport, a clock, a destination, looping, launching, a place on the timeline, export. A separate app would build all of that again, and the two apps could only stay in time through MIDI clock over a loopback, which is looser than sharing one clock (section 8).
2. **A song mixes them.** A step-sequenced beat, a played bass line and a generated texture belong in one arrangement. With clip kinds, you build that arrangement in one place and export it as one thing.
3. **Other sequencers settled on this shape.** Logic Pro puts step sequencer patterns in the same tracks and Live Loops cells as ordinary regions. FL Studio arranges step patterns and piano roll patterns in one playlist. Ableton Live 12 added generators that write notes into a clip. In all three, patterns live beside ordinary clips.
4. **Converting between them is one command.** Make notes turns any pattern or generator into an ordinary notes clip you can edit by hand.

What I considered and didn't recommend:

- **A separate app.** Its one real advantage is a focused, touch-first screen, like a hardware groovebox. I'd get that from MIDI Glass instead: a Glass layout can launch clips and scenes and toggle steps over MIDI once the sequencer has remote control (phase 8). MIDI Glass's own button sequences stay small on purpose. Its design says loops and branches belong in a real sequencer, and this is that sequencer.
- **Track effects only** (arpeggiator, chance, repeat as effects on a track). Useful later, and the natural place to reuse MIDI Patchbay's transform steps, but they change notes that already exist. They don't replace patterns as something you write.

**Pattern clips (comp 3)** in the first version: drum lanes (one note per lane) and a melodic mode (one note per step, with accent and slide rows). Steps per pattern, step size and swing. **Each lane can have its own length**, so a 12-step hat against a 16-step kick drifts and comes back. Per step: on, velocity (16-bit), chance, ratchet (2 to 8 quick repeats, with a shape), condition (every Nth pass, first pass only, not the first pass), nudge (early or late), length, and locks (a controller value for that step only; on a MIDI 2.0 destination a lock can be a per-note controller, so it changes only that hit).

**How pattern clips work, as built** (October 10). A click turns a step on or off, and a drag up or down on a step sets how hard it plays. A right click looks at a step without changing it. The arrow keys move between steps, Space turns one on or off, and the inspector beside the grid has every setting, so the grid works from a keyboard too. A lane's conditions count that lane's own passes, so a step every second pass of a 12-step lane plays every 24 steps. Accent plays at full velocity. Slide makes a note run a quarter step into the next one, so a synth set to glide on overlapping notes slides to it. There's no slide time, because the synth sets that. Vary picks a new seed. **Make notes** swaps the pattern on that track for an ordinary notes clip that plays the same thing, for as many bars as it takes the lanes and conditions to line up (at most 64). Each step's chance stays on its note. The pattern stays in the sequence while another track uses it, and undo brings it back. Lanes are named from the General MIDI drum map, or by their note, or by a name you give them. **Not built yet:** locks, and the comp's choice of lane names.

**Generator clips (comp 4)** in the first version:

- **Euclidean rhythm.** Rings of pulses spread as evenly as possible over a number of steps, each with a rotation, note, velocity and chance.
- **Chance melody.** Notes from a scale within a range, with how busy, note lengths, velocity range, and **Keep from the last pass**: 100% repeats, 0% is new every time, and anything between drifts slowly. That last setting covers the "evolving loop" idea without a separate type.
- **Arpeggiator.** Plays what you hold on the track's source, or another track's notes, in the usual orders, rates and octaves, with latch. While an arpeggiator clip plays, it listens to the track's source whether or not the track is armed.

**How generator clips work, as built** (October 10). New Euclidean rhythm and New chance melody are in an empty slot's menu and in the timeline's. The editor shows a generator's settings where the piano roll goes. A Euclidean rhythm is a table of up to 16 rings. Each ring has its own number of steps, so rings of different lengths drift against each other like a pattern's lanes. A chance melody is a form: key, scale, lowest and highest note, how busy, keep from the last pass, the velocity range and the note length. The header sets the type, the length (1 to 16 bars), the step size and **Changes each pass**. With it off, every pass plays the same, and a ring's chance only decides which hits that one pass has. With it on, each pass rolls again, and a chance melody keeps as much of the last pass as you ask. Vary picks a new seed. Make notes turns a generator into an ordinary notes clip four passes long (at most 64 bars). **Not built yet:** the arpeggiator (an arpeggiator clip keeps its settings and plays nothing), the comp's ring drawing, and its four-pass preview.

Later candidates, in the order I'd look at them: per-lane speed and a fill mode for patterns; polyrhythm and "bouncing ball" generators; a generator that learns note-to-note movement from a clip you give it.

**The rule that makes all of this work: a result depends only on the clip's seed and the pass number.** Chance, conditions and generators are worked out from a random sequence seeded by the clip's seed (saved in the sequence) plus which pass this is. So playing from the start always gives the same hits, jumping back replays what you heard, and export writes exactly that. "Same" uses the same seed every pass. "Changes" mixes in the pass number. Keep from the last pass needs the previous pass, which the engine works out by running the earlier passes forward. That's cheap: it's a few hundred numbers, not sound.

## 5. MIDI 2.0 first

**What a clip stores.** Notes go into a note list at MIDI 2.0 resolution: start, length, note number, channel, 16-bit velocity, release velocity, attribute type and value, per-note pitch bend and per-note controller curves, and chance. Everything else is stored as the Universal MIDI Packet (UMP) message that was recorded or drawn: 32-bit controllers, registered and assignable controllers (RPN and NRPN), program change with bank, pitch bend, pressure, system exclusive, and Flex Data. The group is not stored. It belongs to the destination.

**MIDI 1.0 input.** A MIDI 1.0 device's notes are paired into the note list with their velocities scaled up using the MIDI 2.0 bit scaling spec (M2-115). That spec says scaling a value down after scaling it up gives back the original, so a part recorded from a MIDI 1.0 keyboard plays back to a MIDI 1.0 synth exactly as played. The spec only promises that when one implementation does both directions, which is one reason the app does its own conversion (next paragraph). Other MIDI 1.0 messages are kept as they arrived.

**What goes on the wire is decided per destination.** For each destination (device and group), the app reads the function block, or the group terminal block when that's all there is, and sends MIDI 2.0 or MIDI 1.0 to match. It converts in both directions itself. Windows only downscales for some endpoints, never because of what a function block declares, and it doesn't scale MIDI 1.0 up to MIDI 2.0 at all ([data translation](../../../../docs/kb/data-translation.md)). A per-track setting can force one protocol for a device that describes itself wrongly.

**What MIDI 1.0 can't say.** Per-note pitch bend, per-note controllers and per-note management have no MIDI 1.0 equivalent (UMP spec, appendix D.2.8), and a note attribute such as exact pitch has nowhere to go in a MIDI 1.0 note. A MIDI 1.0 destination doesn't get them. The app says so before it happens: a bar in the clip editor (comp 2), and a count in the export dialog (comp 7). Translating per-note expression to MPE for MIDI 1.0 synths that support it is a reasonable later option. The UMP spec allows for alternate translations like that, but MPE takes over channels, so it has to be a choice.

**Flex Data.** Tempo, time signature, key signature, chord names, lyrics and text live on the Tempo and meter track and in clips, are written to MIDI 2.0 clip files, and can be sent live. Devices that speak MIDI 2.0 can get Set Tempo messages next to MIDI clock (comp 6).

**MIDI-CI.** Program names come from the device over MIDI-CI Property Exchange when it offers a program list. The API already has this (`MidiCapabilityInquirySession.GetProgramListAsync`). Controller names (the controller list resources in M2-117) would let an automation lane say "Cutoff" instead of "NRPN 7:54", but the API doesn't have them yet. That's an API gap to file when we get there. A MIDI 2.0 clip file can also ask for a profile to be on before it plays (section 11), which the sequencer can do on the destination later.

**Note names.** Note 60 is C3, the same as the WinRT API's `MidiMessageHelper` and every other tool in the family. I checked MIDI Glass, MIDI Keyboard, MIDI Monitor, MIDI Patchbay, MIDI Scratchpad and the console, and they already agree, so nothing had to change. Other apps and manuals call note 60 C4, so the editors show the note number next to the name.

## 6. Recording

- **Arm a track (R) and play.** What arrives on its source is recorded, filtered by group and channel and by kind of message. System exclusive is off by default, so a device's chatter doesn't fill clips.
- **Echo while armed.** What you play goes straight to the track's destination, through the same translation as playback, so you hear the sound you're recording for. One echo hop through the service is the latency cost.
- **No keyboard at hand?** The track's source list has **MIDI Keyboard** (comp 5). It sets the track to record from the built-in Default App Loopback (B) and opens Windows MIDI Keyboard playing into Default App Loopback (A), on the track's channel. Arm the track and play with the mouse, touch or the computer keyboard, and echo plays the track's destination.
- **Three places to record:** on the timeline at the playhead (with loop recording that layers or replaces), into an empty launcher slot (it records for the launch length, then loops straight away), or **Capture**: every armed track keeps the last few minutes of what you played, and Capture turns it into a clip, even if you never pressed record.
- **Timing.** Windows timestamps each incoming message when it arrives. The app converts that to a bar and beat with the transport's start time and the tempo map. The device's own delay isn't known, so each source gets an optional "recorded late by" offset.
- Several tracks can record from one source (to layer a part on two synths), and the app opens one connection per source device and shares it.
- Count-in and punch in and out come in a later phase.

## 7. Automation

- **Drawn on the track itself** (comp 1). Lanes open under their track on the timeline, and the arrow beside a track's name shows or hides them. A lane changes one thing: a controller, pitch bend, channel pressure, program change, or a registered or assignable controller (RPN or NRPN). Add one with the + on a lane's header, or with Add automation in the track's menu.
- **Points.** Each point has a position and a value. Values are kept at 32 bits, so nothing is lost on a MIDI 2.0 device. The line ramps from one point to the next, unless a point is set to hold until the next one, which makes a step. Before the first point a lane holds the first value, and after the last it holds the last.
- **Editing.** Click a lane to add a point, and drag a point to move it. Hold Ctrl and drag to draw. Double-click or right-click a point to type a value or make it hold. Delete removes the selected point. Each of those is one undo step. The chip in the lane's header shows the value at the playhead.
- **Clips have lanes too.** A clip's lane travels with the clip and loops with it, which is what a launched clip needs. The file, playback and exports handle clip lanes now. The editor for them comes later. Until then, a clip's lane shows as a dashed line over the track's lane wherever the clip plays.
- **Which one wins.** While a track plays a launched clip, the clip's lanes apply and the track's automation waits. On the timeline, a clip's lane for a controller wins over the track's lane for the same controller, but only inside that clip. The innermost lane always wins, so it's predictable.
- **How often values are sent.** A ramp goes out as steps, no more often than the automation interval set for that device in MIDI Settings (every 10 ms when it has none). The steps sit at fixed beat positions, spaced for the fastest tempo in the sequence, so they never come faster than that interval. A step goes out only when the value changes at the destination's resolution: 7 bits for most controllers on a MIDI 1.0 device, 14 bits for pitch bend, RPN and NRPN there, and 32 bits on a MIDI 2.0 device. So a 7-bit lane can't send more than 128 different values in a ramp, and a dense lane going to a MIDI 1.0 device is thinned on purpose, because a MIDI 1.0 cable carries only about a thousand controller messages a second.
- **Starting part way.** Playing from the middle, a loop going round, an edit while playing and unmuting a track each send every lane's value at that point, so a device is never left where an earlier pass put it.
- **Tempo and meter.** The Tempo and meter track draws the tempo as a line with a handle on each change. Drag a handle to change the tempo or move the change (the first one stays at the start). Double-click one to type a tempo. Right-click the track to change the tempo or meter there, make a tempo ramp to the next change, or delete a change. Selecting the tempo display opens the tempo and meter in effect at the playhead. When a meter change is added or removed, later meter changes stay on the same bar.
- **Exports.** A Standard MIDI File or clip file export includes the automation as ordinary messages.
- **Not built yet:** line and curve tools, and filling a selection with an LFO shape from MIDI Glass and MIDI Patchbay (the shared LFO wave code).

## 8. Clock, sync and timing

**How playback works.** Like the MIDI Player, the app hands each message to the service with the time it's due, and the service sends it then. The engine looks a short time ahead. The catch is that **a message handed over can't be taken back**, so a note you delete, a track you mute or a clip you launch inside that window still plays once. That's also why the engine keeps each note's end until it's almost due, instead of handing it over with the start. An end handed over early can't be taken back either, so after a restart, a jump or a launched clip it would cut off a new note on the same key. The Player looks 250 ms ahead. An interactive sequencer needs to react faster. My hypothesis was 100 ms. Phase 0 measured it (section 20): a message handed to the service arrives within a fraction of a millisecond of its time, whatever the look-ahead, so the service isn't what limits it. What's left is the engine's own thread, which woke up to 2 ms late on an idle PC. So the look-ahead can be much shorter than the Player's. I'd start at 40 ms, and have the engine stretch it when it sees itself waking late on a busy PC, never below 20 ms or above 250 ms. Launches quantized to a bar are exact as long as the bar is further away than that. A launch set to Immediately starts one window from now.

**Each device's offset is applied by the app, for now.** MIDI Settings lets you set how early to send to each device, and some transports work out a value of their own (Bluetooth MIDI and Network MIDI 2.0 do). A newer version of the service's scheduler sends each message that much early, up to one second, but the service in Windows doesn't do that yet, and that's the service people will have when this app ships. So the engine does it. It reads each device's offset by the same rule as that scheduler (the value from MIDI Settings when you chose one, otherwise the transport's), sends that device's messages that much early, and starts playback late enough that the device needing its messages earliest still gets them in time. The look-ahead for each destination is the base look-ahead plus that destination's offset, so a device with a large offset makes edits on its track take effect a little later, and only on that track. The track setup shows each device's offset (comp 5). Once the service applies offsets itself, the app has to stop, or they're counted twice. Nothing tells the app which service it has, and a marker written by the service would only reach versions of Windows that have it (section 22). So it's a setting: **Send early for each device's offset**, under Timing in the clock menu, on by default. The help beside it explains why it's there, and says to turn it off once your version of Windows MIDI Services sends early by itself, which its release notes will say. With it off, a track's setup still shows the device's offset and says that sending early is off.

**Silence** uses the Player's rules, which were measured: note offs for what's sounding, then sustain off before all notes off, then all sound off and pitch bend center, only on the channels the track uses (Windows MIDI Services is multi-client, so the app must not silence other apps), sent at once and again after the look-ahead. Starting in the middle of a song first sends each channel's bank, program, controllers and pitch bend, like the Player's seek.

**Internal clock.** The Tempo and meter track is the clock: tempo changes and ramps, and time signatures.

**Metronome.** With no audio, the click is a note sent to a destination you choose: endpoint, group, channel and note, with its own velocity for the first beat of each bar. By default it plays the General MIDI Synth's side stick, the rimshot-like click at note 37 on channel 10 in the General MIDI drum map, when that synth is present. The engine schedules it like any other note, so it lands on the beat and follows the tempo and meter. It can play always or only while recording. Count-in comes with punch in and out. It's a button on the transport, with its settings in a flyout (comp 6). The settings belong to the PC rather than the sequence, because they name a device.

**Sending clock** (comp 6). MIDI clock pulses are scheduled from the tempo map like any other message, so a ramp and the notes stay locked together. The shared clock generator assumes one tempo at a time, so it isn't the right tool here. Start, Continue and Stop go out with the transport, and Song Position Pointer goes out when you move the playhead. Each device gets its own offset, the same idea as MIDI Clock.

**Clock and sync, as built (October 10).** The clock button opens a Clock and sync dialog. Its settings belong to the PC, not the sequence, because they name devices.

- **Send clock to:** a table with a row for each device: the device, its group, and whether it gets clock pulses, start and stop, and song position, plus its own offset in milliseconds. Stop goes out after the last pulse that was already handed over, so a follower never sees pulses after it stopped. When the timeline loop goes round, followers get a song position for the loop start, just ahead of the next pulse.
- **Also send the tempo itself to devices that speak MIDI 2.0:** a MIDI 2.0 Set Tempo message where playback starts, at each tempo change and once a beat through a ramp, just ahead of the pulse at the same moment. A MIDI 1.0 device gets only the pulses.
- **MIDI Time Code:** one device and group, 24, 25, 29.97 drop frame or 30 frames a second, and the time code at the top of the sequence (01:00:00:00 unless you change it). Time code counts the time on the wall from the top of the sequence, so a tempo change doesn't change it. The engine schedules the quarter frames itself, with the shared time code math, so they stay in step with the notes and the loop. A full frame message goes out when playback starts, when the loop goes round, and where it stopped.
- **Tempo comes from:** this sequence, or another device's MIDI clock (below).

The status bar says where the clock goes, such as "Internal clock, 124 BPM, sent to 1 device". Clock and time code to the same device count as one device. Measured through Default App Loopback on October 10: 203 pulses over 4.07 seconds at exactly 124 BPM, every gap 20,161 microseconds; 100 quarter frames a second at 25 frames a second; a full frame for 01:00:00:00 at the start, and one for where it stopped after Stop; Set Tempo for 124 BPM just ahead of Start; and no note messages, because every track played to a missing device.

**Following another device's clock** reuses the clock follower MIDI Patchbay's LFO uses: it turns pulses into a position and a tempo, and handles Start, Continue, Stop and Song Position Pointer. The honest limit is in the comp: the app can only plan as far ahead as the next pulse it expects, so following is looser than the internal clock. When you can, let the sequencer be the clock. If the clock stops arriving, playback stops (or, by choice, carries on at the last tempo).

**Following, as built (October 10).** Choose **Another device's MIDI clock** in Clock and sync, then the device and group. While following:

- That device's pulses set the tempo, and its Start, Continue, Stop and song position run playback. The sequencer's Play button says playback waits for that device, and the loop is off.
- Each pulse that arrives says exactly where the music is, and the music moves on at the pace of the last few pulses. Nothing is planned further ahead than the pulse after next, so a pulse that comes late, or at a new tempo, can only put that much in the wrong place.
- When the device starts without having sent its clock first, nothing plays until the first pulse arrives and shows where the downbeat is.
- If the pulses stop for eight pulses' time, or three tenths of a second, whichever is longer, playback stops. Carrying on at the last tempo isn't built.
- The status bar says "Following (device), 100.0 BPM", or that it's waiting for that device's clock. Devices that get the sequencer's own clock get the followed tempo passed on.

Measured through Default App Loopback on October 10, with the MIDI console sending 100 BPM: all 41 notes landed exactly on their pulses, and Stop, Continue and the clock going quiet each stopped or restarted playback.

**Not planned: following incoming MIDI Time Code.** MIDI Clock made the same call: it takes a phase-locked receiver and a way to bend the service's clock.

## 9. Files

**The sequence file is JSON** (`.midisequence`, in `Documents\MIDI Sequences`). Reasons: it matches MIDI Glass and MIDI Patchbay, so an assistant can read and write it with an agent guide like theirs; differences between two versions are readable; and the provenance block is shared code. MIDI is small: 100,000 notes and 20,000 other messages make a 4.4 MB file that opens in about half a second (section 20). If a recording-heavy sequence ever opens too slowly, a reader written for this format would be the first fix.

The shape, as phase 1 writes it:

```json
{
  "fileVersion": 1,
  "name": "Night Drive Sketch",
  "provenance": { "author": "Pat Example", "license": "CC-BY-4.0", "digitalSourceType": "composite" },
  "ticksPerQuarterNote": 960,
  "tempo": [
    { "tick": 0, "bpm": 124 },
    { "tick": 61440, "bpm": 124, "rampToNext": true },
    { "tick": 69120, "bpm": 128 }
  ],
  "meter": [
    { "tick": 0, "numerator": 4, "denominator": 4 }
  ],
  "tags": [
    { "tick": 61440, "text": "Chorus", "color": "#F2C14E" }
  ],
  "scenes": [
    { "id": "s-intro", "name": "Intro" },
    { "id": "s-groove", "name": "Groove" }
  ],
  "tracks": [
    {
      "id": "f-synths",
      "kind": "folder",
      "name": "Synths",
      "color": "#9C8CFF",
      "tracks": [
        {
          "id": "t-bass",
          "kind": "track",
          "name": "Bass",
          "color": "#5BC0EB",
          "source": { "endpoint": { "name": "Keystep 37" }, "group": "any", "channels": "any" },
          "destination": { "endpoint": { "name": "Moog One" }, "group": 0, "channel": 1, "protocol": "automatic" },
          "startup": [ "20C15100", "20B10764" ],
          "timeline": [
            { "clip": "c-bass-a", "tick": 15360 },
            { "clip": "c-bass-a", "tick": 30720, "length": 30720 }
          ],
          "slots": [ null, "c-bass-a" ],
          "tags": [
            { "tick": 46080, "text": "Filter opens here" }
          ],
          "lanes": [
            {
              "id": "a-expression",
              "target": "controller",
              "number": 11,
              "channel": 1,
              "points": [ [0, 1503238553], [30720, 3435973836], [46080, 1288490189, "hold"], [61440, 2362232012] ]
            }
          ]
        }
      ]
    }
  ],
  "clips": [
    {
      "id": "c-bass-a",
      "kind": "notes",
      "name": "Bass line A",
      "length": 15360,
      "loop": true,
      "seed": 20266,
      "origin": { "how": "recorded", "detail": "Keystep 37" },
      "notes": [
        [0, 403, 1, 40, 65535],
        [480, 403, 1, 40, 36044, 0, 0, 0, 60]
      ],
      "events": [
        [0, "40B14A00 80000000"]
      ]
    }
  ]
}
```

Groups and channels in the file count from 0, like MIDI Glass and MIDI Patchbay files. People and screens count from 1. Folders hold their tracks, the same way DAWproject nests them. A note is `[tick, length, channel, note, velocity]`, with release velocity, attribute type, attribute data and chance after those only when a note needs them, which keeps a big recording small. Per-note pitch bend and per-note controllers are ordinary messages in the clip, because that's what they are on the wire. Other messages, and start-up messages, are written as their UMP words in hex, the way MIDI Monitor shows them. Their group is ignored, because the destination decides the group. A placement's `length` is only there when it plays longer than its clip, looping. A lane's `target` is `controller`, `pitchBend`, `channelPressure`, `program`, `registered` (RPN) or `assignable` (NRPN). A controller lane has a `number`, and RPN and NRPN lanes have a `bank` and an `index` instead. A lane can also have a `name`. A point is `[tick, value]`, with `"hold"` after them when the value holds until the next point instead of ramping. Values run from 0 to 4,294,967,295 whatever the device takes, and playback scales them down. A track writes `"lanesOpen": false` only when its lanes are hidden. A track or folder writes `"height"` only when someone has made its row taller or shorter (24 to 480), and the sequence writes `"tempoHeight"` for the Tempo and meter row the same way. Clips can have `lanes` too, with ticks counted from the clip's start. Generator settings go in a `settings` object too (below). A file written at another `ticksPerQuarterNote` (by hand, or by another tool) is scaled to 960 when it's read, and keys this version doesn't know are kept and written back.

**A pattern clip** keeps its steps in `settings`. Its notes aren't saved, because they come from the steps:

```json
{
  "id": "c-beat-a",
  "kind": "pattern",
  "name": "Beat A",
  "length": 3840,
  "loop": true,
  "seed": 11,
  "settings": {
    "mode": "drums",
    "stepCount": 16,
    "stepTicks": 240,
    "swing": 54,
    "lanesKeepLength": true,
    "channel": 9,
    "lanes": [
      { "id": "p-kick", "note": 36, "stepCount": 16, "steps": [ [0, 57671], [4, 57671], [8, 57671], [12, 57671] ] },
      { "id": "p-hat", "note": 42, "stepCount": 12, "steps": [ [0, 36044], [9, 19660, { "ratchet": 3, "shape": "softer" }] ] },
      { "id": "p-open", "note": 46, "stepCount": 16, "steps": [ [14, 36044, { "condition": "2:2" }] ] }
    ]
  }
}
```

A step is `[index, velocity]`, counted from 0, with an object after them for anything that isn't the default: `chance` (percent), `ratchet` (2 to 8) and `shape` (`softer` or `louder`), `condition` (`firstPass`, `notFirstPass`, or `"1:2"` for the first pass of every two), `nudge` (ticks, negative is early), `length` (percent of a step, 50 when it isn't there), and in melodic mode `note`, `accent` and `slide`. A lane has a `note` and a `stepCount`, and `name`, `mute` and `solo` when they're set. `mode` is `drums` or `melodic`, and `swing` runs from 50 (straight) to 75. The clip's `length` is written for other readers, but it always comes from `stepCount` times `stepTicks`.

**A generator clip** keeps its settings the same way, and its notes aren't saved either:

```json
{
  "id": "c-euclid",
  "kind": "generator",
  "name": "Euclid 5/16",
  "length": 3840,
  "loop": true,
  "seed": 23,
  "settings": {
    "type": "euclidean",
    "stepTicks": 240,
    "eachPass": "same",
    "channel": 0,
    "noteLength": 50,
    "rings": [
      { "note": 64, "pulses": 5, "steps": 16, "rotate": 0, "velocity": 58982, "chance": 100 },
      { "note": 79, "pulses": 3, "steps": 12, "rotate": 1, "velocity": 52428, "chance": 70 }
    ]
  }
}
```

A chance melody's `settings` look like this:

```json
{
  "type": "chanceMelody",
  "stepTicks": 240,
  "eachPass": "changes",
  "channel": 0,
  "noteLength": 80,
  "scale": "minorPentatonic",
  "root": 4,
  "low": 64,
  "high": 76,
  "busy": 50,
  "keep": 75,
  "velocity": [32768, 62258]
}
```

`type` is `euclidean`, `chanceMelody` or `arpeggiator`. `eachPass` is `same` or `changes`, and `noteLength` is a percent of a step. A ring has a `note`, `pulses` (how many hits), `steps` (1 to 64), `rotate` (how many steps later it starts), `velocity` (16-bit) and `chance` (percent). A chance melody has a `scale`, a `root` (0 is C), `low` and `high` notes, `busy` (about how many steps play, in percent), `keep` (the percent kept from the last pass, used only with `changes`), and `velocity` as the lowest and highest 16-bit values. `scale` is `major`, `minor`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `harmonicMinor`, `majorPentatonic`, `minorPentatonic`, `blues` or `chromatic`. Each type writes only its own settings. The clip's `length` is how long one pass is.

**Rules carried over from the other tools:** a file from an older version is converted with the original copied to an Earlier versions folder (MIDI Patchbay's rule). A file from a newer version is opened read-only rather than saved over, so newer settings aren't lost (the guard MIDI Patchbay and MIDI Glass got in October). A sequence or MIDI file can arrive by double-click from the internet, so every count and length in it is capped and checked, the way the SMF reader already does it.

**Undo.** MIDI Patchbay keeps a whole copy of the patch per undo step. That won't scale to a sequence with a hundred thousand notes, so the sequencer undoes by change: an edit to notes records only the notes it took out and put in, and small things (a track's settings, the tempo map) keep a before and after copy. The history stays under a memory ceiling by forgetting its oldest steps, but always keeps the newest.

## 10. Import

- **Standard MIDI Files** (.mid, .midi, .kar, .rmi, .smf) through the same reader as `MidiStandardFileReader`: formats 0, 1 and 2, karaoke text, chord symbols, and its limits for hostile files. Each file track becomes a clip on a new track (or clips on the selected track). The file's tempo is used when the sequence has none. Its program, volume and pan changes can become start-up messages. Its markers become tags. Its copyright text stays with the clips it came from.
- **MIDI 2.0 clip files** (.midi2). This needs a reader we don't have yet. It's the same reader the MIDI Player is waiting for, and both are waiting on reference files from another implementation to prove they read what others write.
- **DAWproject files** (.dawproject) from Bitwig Studio, Cubase, Studio One and the other apps that support the format (section 12).
- **Captures from MIDI Monitor and the console.** A CSV capture (MIDI Monitor's Save as CSV, or the console's `endpoint monitor --capture-format csv`) has exact times and every UMP message, so a jam you captured plays back exactly as it happened, one track per group. A .mid capture is MIDI 1.0 at 960 ticks per beat. Text captures with UMP words, and MIDI Monitor's .bin files, have no times, so they're laid out one message per step, which suits a set-up dump.
- **Paste.** UMP words copied in MIDI Monitor paste into a clip (comp 7).
- **Captures and paste, as built (October 10).** File > Import reads the CSV that MIDI Monitor and the console save (the console writes words as `0x1234ABCD`, MIDI Monitor without the `0x`), and Open and drag and drop take a `.csv` too. Each group becomes a track that keeps its messages' own channels, made the same way as recording, with times turned into ticks at the sequence's tempo. Clock, stream and utility messages are left out and counted. Ctrl+V, or File > Import > Messages copied in MIDI Monitor, puts MIDI Monitor's Copy as UMP words on the selected track at the playhead as a new clip: one message a step (1/32 to 1/4), or all at once for a set-up dump. A `.mid` capture already comes in as a Standard MIDI File. Code in `CaptureImport.cpp`, 5 tests. Not seen on screen yet. Not built: MIDI Monitor's text captures and `.bin` files.
- A small later change to MIDI Monitor could add "Open in Windows MIDI Sequencer" for a selection.

## 11. Export

**MIDI 2.0 clip files** (the MIDI Association's MIDI Clip File, M2-116-U, the format MIDI 2.0 calls SMF2). From the spec: the extension is `.midi2`; the file starts with `SMF2CLIP`; everything after that is UMP, big endian; any Set Profile On messages come first, then the ticks per quarter note message (the sequencer writes 960); every message after the profiles has a delta clockstamp in front of it; the configuration header may hold one Set Tempo, one Set Time Signature and set-up messages such as bank, program, volume and pan (our start-up messages); the sequence starts with Start of Clip and ends with End of Clip. Tempo changes in a standard MIDI file may only fall where a MIDI clock would (every 1/24 of a quarter note, per the UMP spec), so the exporter puts them there. The sequence name, clip name, composer and copyright go in as the standard Flex Data text messages (the sequence name as the one the spec calls Project Name). MIDI 2.0 has no marker message, so tags go in as the general text message the spec calls Unknown Metadata Text Event.

**Whole sequence.** The MIDI Container File, the format meant to hold several clip files as tracks, isn't published: the MIDI Association's clip file page still says it "will have" one. Until it is, the choices are one clip file per track in a folder (my default), or one clip file with every track on its own group and channel (which loses the track structure and fails when two tracks share a channel). When the container format is published, whole-sequence export should move to it.

**Standard MIDI Files** (.mid, format 1, one track per track) through `MidiStandardFileWriter`, which already turns MIDI 2.0 into MIDI 1.0 by the rules in the data translation article and counts what it had to leave out. The export dialog shows that count before you export (comp 7). Tags become marker events: the sequence's tags in the first track, and a track's tags in that track.

**DAWproject** keeps the most structure of the three (section 12).

**Patterns, generators and notes with chance** are written the way they play from bar 1, with their saved seeds. A file holds notes, not the settings that made them.

## 12. DAWproject

[DAWproject](https://github.com/bitwig/dawproject) is an open format (MIT license) for moving a song between music apps. Version 1.0 is stable, and Bitwig Studio, Studio One, Cubase, Cubasis, VST Live and n-Track Studio support it. A `.dawproject` file is a ZIP holding `project.xml` and `metadata.xml` (UTF-8 XML), plus any audio and plug-in files. Times are in beats or seconds.

It suits this app better than a Standard MIDI File because it keeps the structure. Tracks nest inside folder tracks, the arrangement has clips with loops, the clip launcher has scenes and slots, and there are markers, tempo changes and time signature changes. A note's velocity is a number from 0 to 1, and notes can carry per-note expressions, so MIDI 2.0 resolution survives.

| DAWproject | In the sequencer |
| --- | --- |
| `Track` with `contentType="notes"`, its `name` and `color`, and its `Channel`'s mute and solo | A track |
| `Track` holding other tracks | A folder |
| `Arrangement` lanes: a `Clip` holding `Notes`, with `playStart`, `loopStart` and `loopEnd` | Clips on the timeline |
| `Scenes`, with a `ClipSlot` for each track | Scenes and launcher slots |
| `Note`: `time`, `duration`, `channel`, `key`, `vel`, `rel` | A note, with 16-bit velocity and release velocity |
| `Points` for `channelController`, `pitchBend`, `channelPressure` and `programChange` | Automation lanes and clip lanes, at 32-bit resolution |
| `Points` inside a `Note`, for expressions such as `transpose`, `pressure`, `timbre` and `pan` | Per-note pitch bend and per-note controllers. Not built yet: they're counted and left out (as built, below). |
| The `Transport` tempo and time signature, `TempoAutomation`, `TimeSignatureAutomation` | The Tempo and meter track, with ramps from linear points |
| `Markers` on the arrangement, and markers in a track's lanes | Tags on the Tempo and meter track, and tags on tracks |
| `metadata.xml`: title, artist, composer, copyright, website, comment and others | The sequence name and the provenance block |

**Import.** Note tracks, folders, clips, scenes, automation, markers and the tempo map come in. Audio tracks and clips, plug-ins and mixer settings don't, and the import dialog says how many of each it left out. DAWproject has no MIDI devices, so imported tracks have no destination yet. The dialog lists them so you can choose one for each, with the name of the instrument the track played in the other app as a hint.

**Export.** Tracks, folders, clips, scenes, automation, tags and the tempo map go out. Patterns and generators go out as the notes they play, like the other exports. Linked clips go out as copies: the format can point one clip at another's content, but I'd write copies until we've seen other apps read that. What doesn't fit: destinations (there's nowhere to put a MIDI device, so the destination's name goes in the track's comment), system exclusive and other raw messages (the format leaves low-level MIDI events out on purpose), and registered and assignable controllers (RPN and NRPN). The export dialog counts them before you export, like the Standard MIDI File export.

**Two pieces of work it needs.** The files inside a ZIP are usually compressed. The family now has one ZIP implementation for this, with our own deflate code written from the spec (RFC 1951), and MIDI Glass and the Troubleshooter already use it (done October 10). So import can read compressed files, and export can compress them too. The XML is read with the XML reader built into Windows (XmlLite), which refuses DTDs by default. A file can come from anywhere, so sizes, counts and how far a file expands are capped, like every other import.

**Proving it works** needs files from the apps that write DAWproject. The format's repository has no sample project files (its test data is one audio file), so I'd open our exports in at least one of those apps and import files they wrote.

### DAWproject, as built

Import is on File > Import, and Open and drag and drop take a `.dawproject` file too. Export is on File > Export. The reader and writer (`DawProject.cpp`) don't use XAML, so the unit tests compile them unchanged.

- **Reading.** XmlLite with DTDs refused. A file is capped at 32 MB of project XML, 65,536 zip entries, 1.5 million elements, 96 levels deep and 1 MB of text, and what it makes is capped at the sequence limits. A damaged file and a file that's too large each get their own message. A file that isn't a zip with `project.xml` in it is "not a DAWproject file". Only `project.xml` and `metadata.xml` are ever extracted: audio and plug-in files in the zip are never opened.
- **What comes in.** Note tracks and folders (a track that holds tracks), with name, color, mute and solo. The master, effect returns and VCAs stay out. A track with no `contentType` counts as notes when it has an instrument. Clips on the timeline and in scene slots, in beats or seconds (seconds go through the tempo map). A loop that starts part way in is the same loop turned round, so it becomes a looping clip that starts at its own beginning. Clips that play the same content the same way become one linked clip. Clips inside a clip play as part of it. Notes straight on a track's lane become one clip at the start. Automation of controllers, pitch bend, channel pressure and program change becomes lanes on the track or in the clip, at 32 bits. Markers become tags. The tempo map and meter changes come in, with ramps from linear points, and a tempo point that doesn't say how it moves on holds. The title, artist and copyright come from `metadata.xml`.
- **The dialog.** It lists each note track with a device list, starting on the General MIDI Synth, and the instrument the track played in the other app. Right after the window opens it waits up to 4 seconds for the device list, so the lists aren't empty. Below that, it says what was left out: audio tracks, audio clips, instruments and effects, automation of mixer and plug-in settings, and per-note expressions. Into a new, untouched sequence, the file takes the sequence's place, scenes and all. Into a sequence with work in it, the file's tracks are added and its scenes come after the sequence's own. Either way it's one undo step.
- **What goes out.** Tracks and folders with mute, solo and color. Every timeline placement as its own clip (linked clips as copies). Scenes and slots. Track and clip lanes for controllers, pitch bend, pressure and program. Tags as markers (`Markers` on the arrangement, and `markers` in a track's lanes, as the schema spells them). The tempo and meter maps. A recorded clip's controllers, pitch bend, pressure and program changes go out as automation points that hold, so a recorded sustain pedal isn't lost. Patterns and generators go out as the notes Make notes would give, at most 64 bars. A track with a fixed channel writes that channel on its notes and points, because that's what plays. Each track's comment names its device ("Played on Moog One in Windows MIDI Sequencer"). Times, velocities and values have enough decimal places to come back exactly, and there are no timestamps, so the same sequence always gives the same file.
- **What an export leaves out** is counted in its message, after it's written: system exclusive, per-note and other messages in clips, start-up messages, and RPN and NRPN lanes.
- **Tests.** Seven: a round trip of the sample sequence; the exported XML's comments and escaping; a project shaped like another DAW's (plug-ins, an audio track, a group, the master, shared content, a loop turned round, automation, markers, tempo and meter changes, a scene); seconds on the timeline; patterns; what's counted; and junk (not a zip, no project, not XML, a DTD, too deep, half a file). 155 tests in all.
- **Checked in the app** (October 10): the import dialog, choosing a device, the import, and an export through the save dialog whose file names the right devices. The first run found three defects: the device lists were empty when the file was opened at start-up, four empty scenes came before the file's, and the tempo display didn't change after an import. All three are fixed, and the second run confirmed it. Tag labels now pick black or white text when a color from another app is too strong for the usual dark text.
- **Not built:** per-note expressions in either direction (counted and left out), poly pressure automation, and audio and video beyond counting them. Not yet tried with a file from Bitwig Studio, Cubase or Studio One: the check used a file in the same shape as the format's published Bitwig example.

## 13. Provenance

**The sequence carries the same provenance block as MIDI Glass and MIDI Patchbay** (the shared `ContentProvenance` code): id, version, author, organization, web page, license (SPDX), created, the tool, how it was made (`digitalSourceType`) and the AI disclosure. New sequences fill author and license from the same author profile the other tools use.

**Each clip remembers how it began** (comp 8). That's what lets the app suggest how the whole sequence was made, and it's what you need before you share or sell a piece that uses someone else's file.

| A clip that was | Is described as (IPTC digital source type) |
| --- | --- |
| Recorded from a performance | `digitalCapture` |
| Drawn, typed in, or set step by step in a pattern | `digitalCreation` |
| Made by a generator | `algorithmicMedia` |
| Imported from a file | kept with its `basedOn` source: the file name and the file's own copyright text |
| Written by an AI assistant | `trainedAlgorithmicMedia`, with the AI disclosure (who checked it, and the model when known) |

The sequence as a whole is usually a mix, which IPTC calls `composite`, or `compositeSynthetic` when at least one part was made with generative AI. The app suggests one, and you choose. The shared provenance code now knows `digitalCapture` and `composite` (done October 9). MIDI Glass shows them as "Recorded from a performance" and "Put together from several sources", and the layout and patch agent guides' checkers accept them.

**What reaches an exported file:** a MIDI 2.0 clip file has standard messages for the sequence name, clip name, composer and copyright, and a Standard MIDI File has text events for the same. A DAWproject file's `metadata.xml` has room for the title, artist, composer, copyright, website and a comment. None of them has room for the rest, so the full record stays in the sequence file. Signed packs for sharing sequences (like MIDI Glass's) can come later and reuse the same pack code.

**As built (October 11).**

- File > About this sequence opens the comp 8 dialog (`MainWindowProvenance.cpp`). It uses MIDI Glass's words for the shared fields: Made by, Company or group, Website and License. A website must be `https://` or empty, or Save stays on the dialog and says why.
- The copyright notice for exports is its own field, `"copyright"` in the sequence file. Standard MIDI File, clip file and DAWproject exports all write it.
- The suggestion comes from the clips in use (`SequenceProvenance.cpp`, 5 tests):
  - One kind of clip suggests that kind, and a mix suggests `composite`.
  - Any clip from an AI assistant suggests `trainedAlgorithmicMedia`, or `compositeWithTrainedAlgorithmicMedia` when other clips are there too. That's the term MIDI Glass uses for help from AI, so this replaces the `compositeSynthetic` named above.
  - Imported clips alone keep the default, `digitalCreation`.
- When a source type involves generative AI, the dialog asks how a person was involved and which AI model was used.
- An imported Standard MIDI File's copyright notice goes with each of its clips ("file.mid, “notice”"). When the file replaces a new, untouched sequence, its notice also becomes the sequence's.
- A sequence saved for the first time without a provenance block gets one, filled from Your info for sharing, with the suggested source type.
- "Save these as your info for sharing" updates the profile that all the tools share.
- Seen on screen (`seq-check-info.ps1`): open, reject an `http://` website, save, reopen with the values kept, and undo.

## 14. What it reuses

**Can it reuse the Sequencing WinRT API?** For files and the model, yes. For playback, no.

- **Use:** the Standard MIDI File reader and writer, the sequence model's tempo map, time signature map and bar and beat maths, and `MidiSequenceBuilder` for anything that hands a sequence to other code. In-repo tools compile these shared native sources directly, the way the MIDI Player does, so the app pays no cost for crossing the API boundary on hot paths. This is `src/in-box/Inc/midi_file_sequence.*`, `midi_file_smf_reader.*` and `midi_file_smf_writer.*`.
- **Don't use `MidiSequencePlayer` as the engine.** It plays one linear sequence with a fixed look-ahead. It has no loops, no launching, no edits while playing, no recording and no external clock. MIDI Glass's design already argued against adding a live mode to it, and I agree: a second way of working would make a stable, tested player worse. Instead, the sequencer gets its own engine, built on the Player's measured rules for scheduling, silence and chasing state (section 8).
- **API that could follow, after this release's API lock:** a MIDI 2.0 clip file reader and writer in `Windows.Devices.Midi2.Utilities.Files`, with this app as the first user and the MIDI Player as the second. That's the same way the Player shaped the file reader. A public live-sequencing engine is not something I'd offer until third parties ask for one.

From `midi-app-shared`: the endpoint catalog and endpoint matching (device presence, missing devices), service status, window chrome, settings, the appearance flyout, single instance and file handoff, provenance and author profile, the time code generator, the clock follower, LFO shapes, the channel voice message builders, and later the AI assistant prompt and the pack code. From the MIDI Player: the Composition drawing approach for notes and grids (a pool of visuals sized to what's on screen) and its General MIDI names.

## 15. How it fits with the other tools

- **MIDI Monitor and the console:** their captures import, and MIDI Monitor's copied messages paste (section 10).
- **MIDI Clock:** can be the master clock for the sequencer and your hardware through a loopback, or the sequencer can be the clock (comp 6).
- **MIDI Patchbay:** play a track to a loopback and use Patchbay's steps in between. Later, its transform steps could become track effects inside the sequencer.
- **MIDI Glass:** a touch surface for the sequencer once remote control exists (phase 8): launch clips and scenes, run the transport, toggle steps. The sequencer can make the layout for you (section 16).
- **MIDI Keyboard:** a keyboard button on each track, in the piano roll and in the pattern editor opens Windows MIDI Keyboard already set to that track's destination, group and channel, so you can try a sound without leaving the app. Record from MIDI Keyboard (section 6) opens it on a loopback instead, so what you play is recorded. The sequencer starts `midikeyboard.exe` by its full path in the sequencer's own folder, never through a search path. MIDI Keyboard opens a new window each time it starts, which is fine for a first version.
- **MIDI Player:** plays what the sequencer exports, and shares its reader and drawing approach.
- **MIDI Settings:** not on its toolbar. You said the toolbar is for utilities and setup, and the Player and Clock aren't there either.
- **Loopback Setup:** the destination list can make a loopback without leaving the app (comp 5).
- **AI assistants:** an agent guide for sequence files, like the MIDI Glass and MIDI Patchbay guides, is a later phase. The file format choices in section 9 are partly for this.

## 16. Remote control and MIDI Glass layouts

This is phase 8 work. You asked whether the sequencer could make a MIDI Glass controller for a sequence. It can, and it fits the way MIDI Glass layouts already work.

**Remote control.** The sequencer makes its own MIDI endpoint, named after the app, with the virtual device support in Windows MIDI Services. Anything that sends to that endpoint can control it: MIDI Glass, a hardware controller, or another app. The messages follow one published map, so a layout or a controller preset made for one sequence works with the next: launch a scene, launch a clip by track and scene, stop a track, run the transport, M, S and R for each track, and tempo. The sequencer sends state back on the same endpoint (playing, queued, recording, muted), so a surface can light up what's happening. The details of the map are a phase 8 job. I'd use messages that MIDI 1.0 controllers can send too.

**Make a MIDI Glass layout.** A command writes a `.midilayout` for the open sequence and opens it in MIDI Glass: a pad for each launcher slot, with the track names and colors and a column for each scene, a pad to launch each scene, stop pads, the transport, and M, S and R toggles for each track, all sending to the sequencer's endpoint. It follows the published layout format in the [layouts agent guide](../../../../docs/kb/midi-glass-layouts-for-agents.md) and passes that guide's checker. Because the layout finds the endpoint by name, it keeps working on another PC. Making it again after you add tracks writes a new file and leaves the old one alone, so changes you made in MIDI Glass aren't lost.

What I'll check in phase 8: MIDI Glass can already light a control from a note or controller it receives (its `feedback` block), which covers playing and stopped. Showing queued and recording as different states may need a small MIDI Glass change. A layout holds up to 64 pages of 1,024 controls, so a big sequence spreads over pages instead of making the pads tiny.

**As built (October 10).** Remote control is off until you turn it on in **File > Remote control and MIDI Glass**. Then the sequencer makes an endpoint named **MIDI Sequencer**, with one function block that covers all 16 groups. The map:

- Channels 1 to 15 of group 1 are tracks 1 to 15, in the order of the track list, with folders left out. Tracks 16 and up are on group 2 and up.
- On a track's channel, notes 0 to 63 launch the clips in scenes 1 to 64, note 64 stops the track, and note 65 sends it back to the timeline. Controllers 20, 21 and 22 mute, solo and arm it (64 and up is on).
- Channel 16 is the whole sequence. Notes 0 to 63 launch scenes 1 to 64, 64 stops every track, 65 sends every track back to the timeline, 96 plays, 97 stops, 98 records, 99 turns the loop on or off, 100 turns the metronome on or off, and 101 goes back to the start.

Notes act when they go on, so a MIDI 1.0 pad controller can send all of it. What goes back is the same notes and controllers: a note on at 127 for a clip that's playing or recording and for a transport button that's on, a note off when it isn't, and controllers at 127 or 0. When an app connects, it gets the whole state at once. A clip that sits in more than one slot lights only the slot it was launched from, in the launcher and on the surface. Queued clips aren't shown on the surface yet; that's the MIDI Glass change mentioned above.

**Make a MIDI Glass layout** writes a 1280 by 800 layout in the Studio Dark theme: 8 tracks by 8 scenes a page, up to 9 pages, with the transport and the page tabs in a band that stays on every page. Each track takes the theme's six colors in turn, so the layout follows any theme you switch it to; it doesn't copy the sequence's own track colors, which the layout guide keeps for controls that must be one exact color. It's saved where you choose, starting in `Documents\MIDI Layouts`, turns remote control on, and opens in MIDI Glass. The layout guide's checker finds no problems in it.

Checked on screen and on the wire through a loopback (October 10): the console's play, scene launch, mute and stop messages all acted, the lights came back for each, and the layout opened in MIDI Glass with the sequence's track and clip names.

## 17. Accessibility and keyboard

- Every grid has a keyboard path and a screen reader name: a launcher cell reads like "Bass line A, notes clip, scene 2, stopped", and a step reads like "Step 7, Closed hi-hat, on, velocity 73 percent, ratchet 3".
- No state is shown by color alone: M, S and R are letters, a playing clip has a triangle and an outline, a queued clip says which bar it starts on, and a recording slot says Recording.
- The piano roll can be used without a mouse: arrow keys move between notes, a cursor adds notes, and every value can be typed in the inspector.
- The family's existing rules apply (toggle state changes rather than clicks, names on template roots, the accessibility checker script).

## 18. Performance

- Notes, steps, automation and the timeline are drawn with Composition from pools of visuals, so the cost follows what's on screen, not the size of the sequence. The MIDI Player does this today.
- The engine runs on its own thread. The UI reads snapshots and never takes the engine's lock on a frame. In the MIDI Player, asking the engine from the frame code would have meant about 500 lock waits a second competing with playback, so it keeps its own copy for drawing.
- Targets to measure in phase 0: a 100,000-note sequence scrolls smoothly, 64 tracks by 16 scenes, and 16 tracks of dense automation going to MIDI 1.0 devices.

## 19. Where it lives and what it's called

| | Decided |
| --- | --- |
| Name | Windows MIDI Sequencer. Start menu shortcut: MIDI Sequencer. |
| Executable | `midisequencer.exe` |
| Source | `src/in-box/user-tools/midi-sequencer/`, in the Tools installer with the other tools for now. |
| Settings | `HKCU\Software\Microsoft\Windows MIDI Services\Tools\midisequencer` |
| Sequences | `.midisequence` files in `Documents\MIDI Sequences` |
| MIDI session name | The sequence name, so other tools can show who is using a device |

## 20. Phases

Each phase ends with something you can use.

| Phase | What lands | Status |
| --- | --- | --- |
| 0 | Spikes and measurements: look-ahead against edit and launch latency, how late the engine's thread wakes, JSON size and load time, following a clock from MIDI Clock through a loopback, drawing 100,000 notes. | Scheduling, thread wake-up, JSON and rendering measured (below). Drawing and clock following wait for the window. |
| 1 | The sequence model and file (with tags), undo by change, Standard MIDI File import and export, the MIDI 2.0 clip file writer and reader. A test project like MIDI Patchbay's and MIDI Glass's. No window needed. | Done: 38 tests. The clip file code still needs files from another implementation to prove it reads what others write. |
| 2 | The engine: timeline playback to many destinations, protocol per destination, mute, solo, silence, chase, internal clock, clock out, the metronome. | Done. Looping the timeline and choosing the protocol per group from function blocks landed October 10. The engine also applies each device's offset, because the service in Windows doesn't yet (section 8). 79 tests in all, and a run through the service (below). |
| 3 | The main window: tracks, folders, pins, tags, the timeline, the piano roll, recording with echo, the MIDI Keyboard buttons, light and dark themes, the Tools installer. | Mostly done (October 10). The window builds, runs and plays through the service. Checked on screen: the main window, playback, editing notes in the piano roll with undo, and the track setup panel. Written but not yet checked on screen: recording on the timeline with echo, the MIDI Keyboard buttons, the light theme. The Tools installer's file lists include the app. The icon is a stand-in. Drawing 100,000 notes isn't measured yet. |
| 4 | The clip launcher: scenes, quantized launching, recording into slots, Capture. | Mostly done (October 10). Select an empty slot on an armed track to record into it from the next launch point, for 1 to 16 bars (set in the slot's menu), and the take loops as soon as it ends. Selecting the slot again ends the take at the end of that bar. Checked on screen: recording into a slot, stopping part way, and Capture. Not yet seen on screen: a take looping as soon as it ends. |
| 5 | Automation lanes and tempo map editing. | Mostly done (October 10). Lanes under each track for controllers, pitch bend, channel pressure, program change, RPN and NRPN: click to add a point, drag to move it, Ctrl and drag to draw. Tempo and meter editing on the Tempo and meter track and from the tempo display. Lanes are in the file, in playback (stepped at each device's automation interval, thinned at its resolution, and sent again when playback starts part way) and in exports. The sample sequence has an Expression lane on Bass. 102 tests. Checked on screen: adding, moving and drawing points, undo, the tempo flyout, the add-lane dialog, and hiding lanes. Not built yet: an editor for a clip's own lanes, line and curve tools, and LFO fills. |
| 6 | Pattern clips. | Done (October 10), except locks. Drum and melodic patterns from the launcher's or the timeline's menu, edited in a grid like comp 3 with a step inspector beside it: velocity, chance, ratchet and its shape, condition, nudge and length, and note, accent and slide in melodic mode. Lanes with their own length, swing, step size, Vary and Make notes. The sample's Beat A is a pattern. 116 tests, including one that renders a busy pattern in small windows and matches the whole. Checked on screen: the grid, turning steps on, dragging velocity, the lane menu, adding a lane, and a melodic pattern made from a slot. |
| 7 | Generator clips. | Done. Euclidean rhythms, chance melodies and the arpeggiator, made from a slot's menu or the timeline's, with Vary and Make notes. Their settings take the piano roll's place in the editor. The comp's ring drawing and four-pass preview landed with the update in section 24. Checked on screen. |
| 8 | Following another clock, MIDI Time Code out, remote control, and making a MIDI Glass layout from a sequence. | Done (October 10). Clock out with choices for each device, MIDI 2.0 Set Tempo, MIDI Time Code out, following another device's clock, the Clock and sync dialog, the remote control endpoint, and Make a MIDI Glass layout (sections 8 and 16). Checked on screen and through a loopback. Not built: carrying on at the last tempo when a followed clock stops. 148 tests in all. |
| 9 | DAWproject import and export. | Done (October 10). File > Import and Export, and Open and drag and drop. A dialog picks a device for each track and says what was left out. Recorded controllers export as automation, and patterns and generators as notes (section 12). Checked on screen. Not built: per-note expressions. Not yet tried with a file from another DAW. 155 tests in all. |
| 10 | Provenance screens, capture import and paste, docs, an agent guide, an accessibility pass. | Mostly done (October 11). Built: capture import and paste (section 10), About this sequence (section 13), and the lyrics track (section 23). Also done: the docs page with screenshots, `docs/tools/midisequencer`, which opens by saying the app is a very early preview. The security review bounded loops a file could make run for minutes, and the accessibility and en-US checks found nothing. Not built: an agent guide for `.midisequence` files. 181 tests in all. |

### Phase 0 results

Measured October 9 on a development PC, mostly idle, Release x64. The tests are in `src/in-box/Test/Tools/Midi2.MidiSequencer.unittests` (`MeasurementTests`, and `ServiceTimingTests`, which needs the service and runs only when asked).

| What | Result |
| --- | --- |
| A message scheduled 1 to 100 ms ahead, sent to Diagnostics Loopback A and timed when it arrives on B | Median 10 to 14 microseconds late, 95% within 42, worst 216. No message lost. |
| Sent with no time (immediately) | Median 38 microseconds, worst 106. |
| 200 messages due over the next half second, handed over at once (what an engine sweep does) | Median 11 microseconds late, worst 37. |
| The engine's thread asking to wake in 5 ms | `Sleep`: median 0.58 ms late, worst 1.9. High resolution waitable timer: median 0.51, worst 1.8. |
| The engine on its own thread, playing 16 notes at 240 BPM through the service to Diagnostics Loopback A, timed on B | All 16 on time: median 30 microseconds late, worst 104. Nothing dropped. |
| Working out what one track plays in a 10 ms window, for a 100,000-note clip | 0.0001 ms on average, 0.4 ms at worst. |
| A sequence file with 100,000 notes and 20,000 other messages | 4.4 MB. Written in 10 ms, read in 0.48 s. |
| 400,000 notes and 80,000 other messages | 17.7 MB. Read in 1.9 s. |

What this means:

- **The look-ahead is about the engine, not the service.** The service puts a scheduled message out within a fraction of a millisecond of its time. A 40 ms look-ahead covers the engine waking late with room to spare, so mutes, edits and launches can react about six times faster than the MIDI Player's 250 ms. I haven't measured a PC under heavy load yet, which is why the engine should stretch the look-ahead when it sees itself waking late.
- **JSON is fine for normal sequences.** Reading is where the time goes, almost all of it in the Windows JSON parser. A recording-heavy sequence of 400,000 notes takes about two seconds to open. If that turns out to matter, a reader written for this format would be faster. Writing is quick.
- **Still to measure:** drawing 100,000 notes (needs the window, phase 3), following MIDI Clock through a loopback (phase 8), and a busy PC.

## 21. Risks and unknowns

- **Look-ahead against responsiveness.** 100 ms is a guess until phase 0 measures it.
- **Following a clock.** How loose it is with real devices, and how much the clock follower's smoothing helps, is unmeasured.
- **MIDI 2.0 clip file interoperability.** We can write files that match the spec, but we can't prove other software reads them until we have files from another implementation.
- **The MIDI Container File.** Whole-sequence export waits on a spec with no date.
- **DAWproject interoperability.** Each app that supports it may write it a little differently. We need files from several of them.
- **The deflate decoder** is new code that reads files from anywhere. It has caps, tests with damaged files, and a fuzzing run of 18,000 damaged ZIP files with no errors (October 10).
- **Undo by change** is new work for this family and is easy to get subtly wrong.
- **Large recordings in JSON** might be slow to load. Measure before changing anything.
- **Demand for MPE output.** I don't know how many people would use it. It's a later phase either way.

## 22. Decisions

Your answers to the first round (October 9):

| Question | Decision |
| --- | --- |
| Step sequencers and generators | Clip kinds in this app (section 4). |
| Where it ships | The Tools installer, with the other tools, for now. |
| Names | Windows MIDI Sequencer, `midisequencer.exe`, `.midisequence` files in `Documents\MIDI Sequences`. The app calls the document a sequence. |
| Linked clips | By default, with Make a copy to break the link. |
| File format | JSON, because it's easy to read and parse. |
| Whole-sequence MIDI 2.0 export | One clip file per track until the MIDI Container File is published. No date is known. |
| Note names | C3 = 60 across the family and the WinRT API. They already matched, so nothing changed. |
| Look-ahead | Hypothesize, then measure, and allow for each device's offset from MIDI Settings (section 8). |
| Metronome | In the first version: endpoint, group, channel and note, defaulting to the General MIDI Synth's side stick. |
| Provenance terms | `digitalCapture` and `composite` added to the shared code (done). |
| Look | The family's light and dark themes, background color and backdrop, and as close to the comps as possible. |
| Also asked for | DAWproject import and export (section 12), tags (section 3), a MIDI Glass layout made from a sequence (section 16), and MIDI Keyboard buttons (section 15). |

Your answers to the second round (October 10):

| Question | Decision |
| --- | --- |
| Device offsets | The service in Windows doesn't apply them yet, so the engine does (section 8). |
| A deflate decoder for DAWproject import | Our own, and one ZIP implementation for the whole family. Done: MIDI Glass and the Troubleshooter use it, and the Troubleshooter no longer runs `tar.exe`. |
| MIDI Keyboard windows | A new window each time it starts is fine for now. |

Your answer to the third round (October 10):

| Question | Decision |
| --- | --- |
| How the app finds out that a service applies device offsets | It doesn't. A marker written by the service would only reach versions of Windows that have it, and these apps mostly won't be built into Windows. So sending early is a setting in the app, on by default, with help that says when to turn it off (section 8). Done. |

Nothing is open right now.

## 23. Update, October 11: a lyrics track

Pete asked for an optional lyrics track, looking at how Standard MIDI Files and MIDI 2.0 carry lyrics, and at how apps use lyrics today beyond karaoke.

### What the files say

- **Standard MIDI Files: the Lyric meta event (FF 05), as RP-017 defines it (1997).** One syllable per event, placed when its note starts. A space at the end of the text ends a word; no space means the word goes on in the next event. A carriage return (0x0D) ends a line and a line feed (0x0A) ends a paragraph. An event with no text is a melisma: the last syllable keeps being sung. A lyric event at bar 1 turns lyrics on in some players, a line should break within 40 characters, and `\ [ ] { }` are kept for later use.
- **RP-026 (SMF Language and Display Extensions)** uses those characters: `\r`, `\n` and `\t` as escapes, ruby (small reading text above a word, for Japanese) in `[ ]`, the character set in `{@LATIN}` or `{@JP}` (a byte order mark means Unicode), and song details in `{#Title=...}`, `{#Composer=...}`, `{#Lyrics=...}` and `{#Artist=...}`, closed by `{#}`.
- **Karaoke `.kar` files** (Soft Karaoke) put the words in ordinary text events (FF 01) instead, usually in a track of their own. They announce themselves with an `@K` text event, use `@` lines for the title and language, and start a syllable with `/` for a new line or `\` for a new paragraph. The shared Standard MIDI File reader already recognizes them.
- **MIDI 2.0: the Lyric Data message** (UMP and MIDI 2.0 Protocol 1.1, section 7.5.10). It's a Flex Data message (status bank 0x02, status 0x01) holding UTF-8 text, with the same rules: one syllable per message at the start of its note, a space ends a word, a carriage return ends a line, a carriage return and line feed end a paragraph, and a complete packet filled with 0x00 is a melisma. Lyrics Language (status 0x02) gives a BCP 47 language tag, and Ruby and Ruby Language (0x03 and 0x04) carry ruby. A Lyric Data message is addressed to a channel or a whole group, so one song can carry lyrics in two languages at once.
- **MIDI Clip Files** (M2-116) hold those same Flex Data messages, with every packet of one syllable at one delta clockstamp.
- **DAWproject** has no place for lyrics.

### How apps use lyrics beyond karaoke

- **A lyrics track and a lyrics display.** Studio One 6 added both: words typed into a track over the arrangement, and a big lyrics display for the singer and band on stage. That's the closest model for this app.
- **Words on notes, for notation.** Logic Pro, Dorico, MuseScore, Sibelius and Finale attach each syllable to a note, with hyphens and extender lines for melismas, and MusicXML carries them (`<lyric>` with begin, middle, end and single syllables).
- **Singing synthesis.** Vocaloid, Synthesizer V and UTAU sing whatever syllable sits on each note, so the lyric is part of what the instrument plays.
- **Lyrics in time with the music, outside music apps.** LRC files (one time stamp per line) for music players, subtitles for lyric videos and live streams, and the time-synced lyrics on streaming services.
- **Chord charts and lead sheets** for bands and worship teams, with chords above the words, and presentation software that shows the words on screens.
- **Arranger keyboards** that read lyrics from MIDI files and show them on their screens.

### The design

- **Model.** An optional lyrics track on the sequence: a language (BCP 47, such as `en` or `ja`), the track that sings the words, and syllables. Each syllable has a tick, its text, and what comes after it: more of the same word, the end of a word, the end of a line, or the end of a paragraph. A syllable with no text is a melisma.
- **Where it shows.** A Lyrics row under the Tempo and meter track, with each syllable at its moment. While the sequence plays, a lyrics bar shows the line being sung and the next one, with the current syllable lit, so the app works as a teleprompter.
- **Editing.** Type the words: a hyphen between syllables (`Hap-py birth-day`), a space between words, a new line for each line, a blank line between verses, and `_` for a melisma. Then choose the track that sings them, and the syllables go on that track's notes in order (notes that start together count once). With no track chosen, they go a beat apart from the playhead. Opening the editor again gives the same text back, and every change is one undo step.
- **Files.** The sequence file keeps the lyrics. Standard MIDI File import reads lyric events and karaoke text; export writes RP-017 lyric events. MIDI 2.0 clip file export writes Lyrics Language and Lyric Data messages. LRC export writes a line per time stamp, for music players and video tools. DAWproject export leaves lyrics out and says so.
- **Playback.** Lyrics go out as MIDI 2.0 Lyric Data messages to the singing track's device, when it speaks MIDI 2.0, so a display or a singing synth can follow along. MIDI 1.0 has no message for lyrics, so a MIDI 1.0 device gets none.
- **Not in this version:** ruby, lyrics in two languages at once, and dragging single syllables on the Lyrics row.

### As built (October 11)

- **Code.** `Lyrics.h` and `Lyrics.cpp` hold everything that isn't on screen, with 13 tests in `LyricsTests`. The row is drawn in `ArrangeRendererLyrics.cpp`, and the header, menu, editor, lyrics bar and LRC export are in `MainWindowLyrics.cpp`.
- **The sequence file.** The top-level `"lyrics"` key holds `language`, `track` (the id of the track that sings), and `syllables` as `[tick, text, end]`, where `end` is `"word"`, `"line"` or `"verse"`, or left out when the word goes on. An empty text is a melisma. Keys this version doesn't know are kept.
- **Typing.** A hyphen splits syllables, two hyphens are a hyphen in the word, `_` is a melisma, a new line ends a line, and a blank line ends a verse. A TextBox ends its lines with a carriage return alone, so that counts as a new line too.
- **Placing.** Syllables go on the singing track's note starts, as its timeline plays them, counting notes that start together once. When the notes run out, the rest go a beat apart. Editing words that were already placed keeps their times, and puts any new ones on the notes after the last.
- **Standard MIDI Files.** Import reads the track with the most lyric events, or the words of a karaoke file, and picks the track whose notes start where the most syllables do. It undoes RP-026 escapes, and drops tags and ruby. It reads UTF-16 with a byte order mark, Shift-JIS after `{@JP}`, UTF-8 when the text is valid UTF-8, and Windows-1252 otherwise. Export writes UTF-8 lyric events on the singing track, and escapes `\ [ ] { }`. If that track is left out of the export, so are the words.
- **MIDI 2.0.** Every Start and Continue packet is a full 12 bytes, because a 0 byte marks the end of the text. That can split one UTF-8 character across two packets. The language goes out before the first syllable, and again at each start, jump and loop. A launched clip sends no lyrics, because the lyrics follow the timeline.
- **LRC.** One `[mm:ss.xx]` line for each line of lyrics, with `[ti:]` and `[la:]` tags, in UTF-8 with a byte order mark. An empty time stamp clears the words between verses when there's a gap.
- **Seen on screen** with a probe (`seq-check-lyrics.ps1`, outside the repo): a song file with RP-017 lyrics comes in with its Lyrics row, and the words light up as it plays. The editor changes a word and keeps the times. Undo works, LRC export goes through the save dialog, and remove and undo work three times in a row.
- **Not built:** reading Lyric Data back from a clip file. Those messages come in as ordinary clip events, and they still play.

## 24. Update: changes from the first video review

A reviewer's first look at the app asked for four things: more of it should work with the mouse, the layout should be adjustable, the generator editors should look like comp 4, and there should be a mixer. He also found that MIDI Keyboard, opened from a drum track, showed no program names. This section says what changed.

### Layout

- **Columns.** The edge between the track names and the timeline drags, and so does the edge between the timeline and the launcher. With keyboard focus, the arrow keys move them. A double-click puts each one back. Both widths are app settings, not part of the sequence.
- **Rows.** Every track and folder, and Tempo and meter, has a grip along its bottom edge. Heights run from 24 to 480. At 36 or less, a header shows its name on one line. **Compact** (24) is in a track's menu.
- **Several tracks at once.** Ctrl and Shift select more than one track. Compact and dragging work on all of them.
- **In the file**, heights are `"height"` and `"tempoHeight"` (section 9). They're how the window looks rather than the music, so changing them marks the file as changed without being an undo step, the same as opening or closing a folder.

### Dragging

The reviewer's rule was "anywhere you can move or place things, dragging them should move them."

- **Tracks** drag into folders, out of them, and between rows, several at once, in the order they had. A folder can't go inside itself, and nothing nests deeper than a file allows. A drop is one undo step.
- **Clips** drag along the timeline, to other tracks, between launcher slots, and between the launcher and the timeline. Ctrl places the same clip again instead of moving it. A drag from the launcher to the timeline, or the other way, places the clip and leaves the original where it was. A faded copy shows where it will land, and nothing changes until it's let go.
- **Tags** drag along the top of Tempo and meter. Right-click renames one, changes its color or deletes it. Double-clicking the strip adds one.
- **The loop** drags on the ruler. Its edges set the start and end, and the middle moves it.
- **Tempo changes.** A click on the tempo line adds one, a drag moves it, and a double-click deletes it. A double-click deletes an automation point too, unless that same click made it.
- **Pattern lanes** drag by their names to a new place, with a line where they'd go. A click on a name still opens the lane's menu, and right-click opens it at once.

### Generators, now like comp 4

- **The header** has the type on the left, from a list that says what each one does, with the length in bars and the step size. On the right are the seed, a button for a new one, **Each pass** (Same or Changes) and Make notes. The seed shows in hex and can be typed. An arpeggiator's step size is its **Rate**, next to its octaves and note length as in comp 4. It shows the seed only with a random order, the only time it uses one.
- **A Euclidean rhythm** draws its rings next to a table of them. Dragging around a ring turns it, and the mouse wheel adds or takes away hits. Two bars of what it plays go next to the table when there's room, and under it when there isn't.
- **A chance melody** has its settings down the left, with sliders for how busy it is and how much it keeps. Its first four passes are on the right, with new notes outlined.
- **The inspector** is hidden for generators, because everything it showed is in the panel now.

### The mixer

The reviewer said: "It does not have any MIDI mixer so you need to use the classic CC's." A MIDI mixer is a strip for each track with the controllers that General MIDI synths and most others respond to: volume (CC 7), pan (CC 10), expression (CC 11), reverb send (CC 91) and chorus send (CC 93). With it, nobody has to know those numbers or draw a lane to set a level.

- **As built.** **Mixer** on the transport bar shows the strips where the editor goes. Each control shows the track's lane at the playhead when it has one, and the track's start-up value when it doesn't. When the track sets neither, it shows the General MIDI starting value, faded.
- **Moving a control** sends the value straight away, on the track's channel. It's kept on the lane at the playhead, or as the start-up value when there's no lane. While the sequence plays, a control with a lane is only heard, because the lane plays over it.
- **W** writes automation. While the sequence plays, moves go into the track's lane for that controller, which is made if it isn't there. A point goes in every 64th note, replacing what the lane had there, and the engine plays what's been written as it goes. Each gesture is one undo step. **M** and **S** are the track's own mute and solo.
- **Not built:** strips for folders, meters (the sequencer never hears the audio), MIDI 2.0 per-note controls, and choosing other controllers for a strip.

### MIDI Keyboard program names

**Record from MIDI Keyboard** points the keyboard at Default App Loopback (A), so the sequencer can record what it plays. The keyboard asked that endpoint for its program list, and a loopback has none. So the reviewer's drum track, playing on the General MIDI Synth, showed no names. MIDI Keyboard now takes `--programs-from <endpoint id>` and `--programs-group <1-16>`. It asks that endpoint for the program list over a second connection, while notes and program changes still go where it plays. The sequencer passes the track's destination.

### Proposed next

- **Setting a ring's note from the roll.** Clicking a key on the left of What it plays would give the selected ring that note, and play it.
- **A chance melody's range drawn on the keys.** Two handles on the pass rolls' keys, instead of two number boxes.
- **Make notes from one pass.** Right-click a pass in the preview to keep just that one as a notes clip.
- **Lane names in the pattern header**, as in comp 3 (General MIDI drums and others), and comp 3's legend under the lanes.
- **Moving a meter change by dragging it.** Today it's set and removed from the Tempo and meter menu.
- **A small volume and pan in each track header** when the row is tall enough, so the mixer isn't the only place to set them.

## 25. Update, October 17: Windows MIDI Services device properties and Studio setup

The ask: show what Windows MIDI Services knows about any device and use it throughout the app, the picture and description most of all, and also the automation interval and the function blocks (or group terminal blocks when there are none). And let each destination in use choose whether it gets the clock, start and stop, tempo and the rest, after the fact, in a Studio setup popup, rather than the way many DAWs make you set up the studio before you start.

### What was already there

The engine already used three of these, but nothing showed them: what each group speaks, from its function block or group terminal block (section 5), each device's offset (section 8), and its automation interval for ramps (section 7).

### Device details

- **Any device.** An info button next to Record from and Play to in the track setup, on every Studio setup row, and on every other device Studio setup lists opens a flyout about that device.
- **What it shows.** The picture and description from MIDI Settings, the transport and the protocol the endpoint settled on, and what in the sequence uses it. Then its groups: each function block, or each group terminal block when it has none, with its groups, which way its messages go, and whether it speaks MIDI 2.0, MIDI 1.0, or MIDI 1.0 at 5-pin cable speed. Then how the sequencer sends to it: the offset and where it came from, the automation interval, and whether MIDI Settings says it takes MPE or needs real note offs. Last, its identity: manufacturer, serial number, product instance, USB ids, system exclusive id, family and model, software version and UMP version, where it has them.
- **The same rules as the engine.** The details say MIDI 2.0 or MIDI 1.0 for a group by the same rules the engine uses to choose what to send, so what they say is what's sent.
- **Read off the UI thread.** Everything past the picture and description asks the service, so it's read in the background and added when it's back. The values MIDI Settings writes are read from the endpoint's property bag, not the newest SDK members, so an older copy of the SDK in Windows can't be asked for something it doesn't have.
- **MIDI Settings.** The flyout says where to change a device's name, picture, description, offset and automation interval, and opens MIDI Settings. MIDI Settings has no way to open at one device yet, which would be a good addition.
- **Needs nothing from the engine.** The service doesn't act on "requires note off translation" or "supports MPE". They're hints for apps. The sequencer always sends real note offs to MIDI 1.0 devices, so the first needs no change. MPE would matter once per-note expression can be translated to MPE (section 5).

### Pictures and descriptions throughout

- **Device lists** show each device's picture beside its name: the customer's own from MIDI Settings, otherwise the transport's default (USB, Bluetooth, network and so on). The description is the item's tooltip. An item stays one line, because a ComboBox draws its closed box with the same template as its items.
- **Track headers** show the picture of the device a track plays to, or records from while it's armed, when the row has room for two lines. A track without one keeps the space, so names line up. The description is the tooltip on the device line.
- **The track setup** shows the description under each device list.
- **Mixer strips** show the whole destination and the description as a tooltip.

### Function blocks and group terminal blocks

- **Which port.** On a device with several destination groups, such as an interface with several MIDI ports, a track's device line uses the block's name for its group ("mioXL · DIN 3 · Ch 1") instead of a number. Only when no other group has the same name, because one block can span several groups.
- **Group lists** already used block names. Studio setup rows show each group's name and protocol.

### The automation interval

- **Mixer moves** go to a device no more often than its interval. A move that comes sooner waits, the newest waiting move always goes, and building the strips again doesn't lose one. A device without an interval gets every move, as before.
- **Lanes** say "every 20 ms" when the device has an interval, with a tooltip that explains it, and the details say how often ramps go out either way.

### Studio setup

- **The rows.** One for each device and group the tracks play to or record from, in track order, with the tracks that use it. A track that records from any group gets a row for each group its device sends on. Then the metronome's device, then the clock the sequence follows, then devices only the PC's settings name, then where time code goes. A row that's in use stays even with nothing ticked.
- **The choices.** Clock, Start and stop, Song position and Tempo, and a clock offset in milliseconds. Tempo is off for a group that speaks MIDI 1.0. These were already in the engine for each device. Tempo used to be one switch for every device that got the clock. Now it's a choice for each one, and devices saved before keep what that switch said.
- **Add a device** adds a row with device and group lists, for something no track plays to, such as a drum machine that only follows the clock. Those rows, and rows only the settings name, can be removed.
- **Other MIDI devices on this PC** lists every other endpoint with its picture, description and an info button, so any device can be looked at.
- **Where it's kept.** The PC's settings, the same list Clock and sync kept before, because the choices describe the studio rather than the music. Every sequence that plays to a device uses them.
- **Clock and sync** keeps where the tempo comes from, MIDI Time Code and Timing. Its table of devices became a line saying where the clock goes and a button that keeps what's chosen and opens Studio setup.
- **Opening it.** File > Studio setup…, the device count in the status bar, and the button in Clock and sync.
- **As built.** The rows come from a function with no XAML (StudioSetup.cpp), tested on its own: order, merging the saved choices, the first saved choice winning, the metronome and time code sharing a device's row, and choices that come back on the same rows.

### Play to, Record from and MIDI Monitor

- **The ask.** Turn each group the sequencer uses on or off, each way it goes, and open MIDI Monitor on a group that sends. The worry behind it: devices, often Arduino projects, that hang when an app sends them something they don't expect, such as the MIDI-CI Discovery or Identity Request some DAWs send every port when they start.
- **What the sequencer sends unasked.** Nothing. It never sends MIDI-CI Discovery or an Identity Request on its own. Section 26 adds a button that asks a device for its sound names over MIDI-CI, and only that click sends it. It opens only the devices the sequence uses (where tracks play, the sources of armed tracks, the metronome, and the clock and time code devices), and sends nothing until play, an audition, a mixer move or MIDI Keyboard. So Studio setup isn't too late: when it's up, nothing has been sent to a device just because it's plugged in.
- **Play to off.** The output leaves out every message for that group: notes, clock, tempo and time code alike. A device with every group it's used on turned off isn't opened at all. Turning a group off while the music plays stops it first, so the notes that group holds end while it can still hear them.
- **Record from off.** What arrives on that group is dropped before recording, echo, the arpeggiators or the followed clock see it. A source with every group in use turned off isn't opened.
- **Which rows get which switch.** Play to for a group the device receives on, Record from for one it sends on. A device that isn't here goes by what uses it. Clock, start and stop, song position, tempo and offset are only on groups it plays to, and wait while Play to is off.
- **Shown where the device is.** Track headers, the editor's footer and mixer strips add "(turned off)", group lists mark turned-off groups, and the track setup and device details say so. Clock and sync leaves a turned-off group out of where the clock goes.
- **MIDI Keyboard.** It always runs MIDI-CI Discovery on the device it opens on, and on the one it gets program names from. The sequencer won't open it on a group whose Play to is off, and won't name a turned-off group as where program names come from. MIDI Keyboard still asks whatever device it's opened on by other means.
- **MIDI Monitor.** An eye button opens it on a device, and on the group where there's one: on each Studio setup row for a group that sends, on each other device that sends, and in the device details. MIDI Monitor sends nothing.
- **Where it's kept.** The PC's settings, beside the clock choices, only for groups with something turned off.
- **As built.** The same tested function makes the rows, now with sources, and keeps the switches. Which groups are turned off, and which group a message is for, are tested there too.

### Not built

- An app can't tell whether a device wants clock, or start and stop, from Windows MIDI Services. MidiDefs.h already lists "can receive MIDI clock" and similar as properties MIDI Settings could set for every app to read. That would need the service, the API and MIDI Settings, so it's an API gap for later.
- Clock while stopped, for devices that keep their arpeggiators and LFOs in time between takes.
- Time code to more than one device. The engine can do it, but the setting holds one.
- A way for MIDI Keyboard to skip MIDI-CI for a device, such as not asking a MIDI 1.0 device, or a switch. That's a change to MIDI Keyboard, not the sequencer.

## 26. Update, October 17: recent sequences, track sounds and the MIDI Mixer

The ask: keep the last 100 sequences for an Open recent popup, newest first, with pins kept outside the sequence files. Let the start-up messages in the track setup wrap. Give a track a way to pick its patch: an optional program change before the track plays, chosen from the device's own list when it speaks MIDI-CI. Rename Mixer to MIDI Mixer. Make pan read left, center and right, with no fill from 0. And make the mixer look like the ones in other DAWs: comp 9 (9-mixer.html), approved as shown.

### Recent sequences

- **The list.** Every sequence opened or saved goes to the front. The newest 100 unpinned ones are kept, and the oldest drops off. Pinned ones never drop off, up to 100 more. Paths compare without case, with / and \ the same.
- **Open recent…** on the File menu (Ctrl+Shift+O) opens a popup: a find box, then Pinned and Recent, newest first in each, with when each was last used, a pin, and a remove button. A file that's gone is marked after a check in the background, so a slow network drive doesn't hold up the popup. Opening it says where to look instead of failing quietly.
- **Which window.** Like Open…, a sequence opens in this window when it holds an untouched new sequence, and in a new window otherwise. The sequence already open in this window isn't opened again.
- **Where it's kept.** The app's settings for the Windows account, beside its other settings, not in the sequence files. Sharing a file shares no history. Two windows share the list: each change reads it again and writes it back under a named lock, so one window doesn't undo another's pin.
- **As built.** RecentSequences.cpp has no XAML, and 11 tests cover order, trimming, keeping pins, the pin limit, case and slashes, and reading a damaged list.

### Track sounds

- **Was it there?** Partly. A track's start-up messages already went out each time playback starts, but the track setup only showed them as raw words, and nothing chose a program. Comp 5 drew a Program row that was never built.
- **Where it is.** Track setup, under Start-up messages: a Sound button that shows the sound's name, or "No sound chosen". The rest of the start-up messages are now listed in words, and wrap.
- **Optional.** Off by default, and the device keeps whatever sound it has. When on, the start-up messages begin with one MIDI 2.0 program change, with the bank in it when one is chosen. A device that speaks MIDI 1.0 gets bank select MSB and LSB, then the program change, from the same translation as everything else. A program change or bank select already in the start-up messages, such as from an imported file, is read as the track's sound, and replaced when a new one is chosen.
- **Names.** The General MIDI names from midi-app-shared, the same list MIDI Keyboard uses, with drum kits on channel 10. The dialog says they're General MIDI names, which another device may not follow. The name picked is saved with the track ("soundName"), so a device's own name still shows later without asking again. A number typed in has no name.
- **Hearing it.** Choosing a sound sends it to the device right away. Cancel sends the old one back.
- **Asking the device.** "Ask … for its sound names" sends MIDI-CI Discovery, then asks for the program list, with the same code MIDI Keyboard uses. The sequencer asks only on that click. It never asks a group whose Play to is off, or a device that isn't connected. The answer is kept for that device, group and channel while the window is open. Once it has asked, the sequencer answers Discovery as itself, with model number 4 (MIDI_DEVICE_FAMILY_MODEL_NUMBER_MIDI_SEQUENCER, a new constant in MidiDefs.h), until the window closes. So section 25 still holds: nothing goes to a device that nobody asked for.
- **As built.** TrackSound.cpp has no XAML, and 8 tests cover reading a sound back from start-up messages, setting it, banks, the saved name, and that a MIDI 1.0 device gets bank select and then the program change.

### MIDI-CI code shared with MIDI Keyboard

- MIDI Keyboard's MIDI-CI presence and program list query moved to midi-app-shared, as MidiCiAppPresence and MidiCiProgramListQuery. They were renamed because Inc\MidiCiProgramList.h is found first on the include path under the old name. MIDI Keyboard keeps two small headers with the old names and passes its own model number. Each app sends MIDI-CI errors to its own log.
- MIDI Keyboard should behave as it did. It builds, but it hasn't been run against a MIDI-CI device since the move.

### The MIDI Mixer

- **The name.** "MIDI Mixer" on the button, the panel and the docs, which also say it sends MIDI to each device rather than mixing sound.
- **The strip (comp 9).** Small knobs for expression, reverb and chorus. A larger pan knob, with L, C or R beside it. A long volume fader with a scale from 0 to 127, and the mark at 100 where General MIDI synths start. A meter beside the fader, the volume as a number, then M, S and W. At the bottom, the track's name on its color, in black or white, whichever has more contrast (WCAG). The selected track's strip has an outline in the accent color, and selecting the name selects the track.
- **Pan.** Reads L24, C or R18, and a screen reader says "left 24". The lit arc starts at the center, so nothing fills from 0.
- **Still sliders.** Each knob and the fader is a Slider whose template only shows what the code draws, so the keyboard, focus and screen readers work as they do for any slider. A drag moves from where the control was, so pressing it never makes it jump. Shift makes a drag four times finer. The wheel moves 2 steps, or 1 with Shift. A double-click puts the control back where General MIDI synths start.
- **A gate meter.** It had to be right or not be there. MIDI tells the sequencer when a note starts, how hard, and when it's let go. It doesn't say how long the sound rings. So the meter shows the velocity of the notes the track holds, from the engine's own note on and note off times, and drops at note off. Each note stays 120 ms after it ends, so a short one still shows. It shows nothing while stopped, and nothing for what's played in live. Its tooltip says so.
- **Colors.** The sizes and shapes are comp 9's, but not its plain grays. Pete asked for the strips to take on the app's colors, so the cards, knob bodies and grooves are see-through, like the editor, Track setup and the launcher's cells, and pick up the window's tint. The volume readout uses the same LCD fill as the position and tempo displays, and M, S and W match the track headers' buttons. The fader caps stay solid. The strips are built again when the theme changes.
- **As built.** MixerSlots.h holds the controllers and how each value reads. MainWindowMixerStrip.cpp draws the strips. The engine now keeps each note's velocity and lets go of it 120 ms after it ends, for PlaybackEngine::HeldNoteLevels, with a test (TheMeterShowsOnlyHeldNotes).

### Not built

- Sound names from a device that doesn't speak MIDI-CI, such as from a file of names.
- A meter for what's played in live, or one that knows how long the sound rings. The second can't be done from MIDI alone.
- Asking every device for its names at once. Each track asks on its own, and only when someone selects Ask.
