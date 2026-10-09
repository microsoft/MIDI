# Windows MIDI Sequencer: design

This is the first design for **Windows MIDI Sequencer**, a new app in the Windows MIDI Services tool family. It records and plays MIDI, MIDI 2.0 first, and it combines clip launching, step patterns, generators and an ordinary timeline in one window. The comps are the HTML pages in this folder (pictures in `shots/`). Nothing is built yet, and this document is for deciding what to build.

It covers what the app is and isn't, how a project is organized, my answer to the question of where step sequencers and generators belong, how MIDI 2.0 shapes the design, recording, automation, clock and timing, files, import and export, provenance, what we can reuse (including the Sequencing API question), how it fits with the other tools, phases, risks, and the decisions I need from you. It doesn't cover code structure below the level of "which engine owns what". That comes with phase 1.

## 1. What it is, and what it isn't

- **It records and plays MIDI. That's all it does with sound.** No audio engine, no WASAPI or ASIO, no plug-ins, no mixer. The sound comes from your devices, from the General MIDI Synth in Windows MIDI Services, or from another app reached through a loopback.
- **MIDI 2.0 first.** Clips hold MIDI 2.0 data at full resolution: 16-bit velocity, 32-bit controllers, per-note pitch bend and per-note controllers, note attributes such as exact pitch. MIDI 1.0 devices get MIDI 1.0, and the app says ahead of time what they'll miss (section 5).
- **One project per window.** Tracks run down the left. Each track has a row of clip slots (the clip launcher) and a timeline, side by side, sharing the same row.
- **Three kinds of clip:** notes (recorded or drawn), pattern (a step sequencer) and generator (notes worked out as it plays, from settings and a seed). All three launch, loop, link and sit on the timeline the same way.
- **Any number of tracks and clips, within reason.** Limits exist only to protect against a damaged or hostile file, and they're set far above what a person builds. The real numbers come from measuring in phase 0, not from a guess.

It isn't a notation editor, a DAW, a router (MIDI Patchbay does that) or a control surface (MIDI Glass does that). If we build something with audio later, it should open these projects. That's one of the reasons the file format is plain and documented (section 9).

## 2. The comps

| Comp | What it shows |
| --- | --- |
| [1-main.html](1-main.html) | The main window: pinned rows, folders, the launcher beside the timeline, a track playing a launched clip, linked clips, automation drawn on a track, recording into a slot, the clip editor at the bottom. |
| [2-piano-roll.html](2-piano-roll.html) | A notes clip in its own window, playing to a MIDI 2.0 destination: per-note pitch bend drawn on the notes, exact pitch, 16-bit velocity, per-note pan, ghost notes, a scale, per-note chance. |
| [3-pattern-clip.html](3-pattern-clip.html) | Pattern clips: lanes with their own length, per-step chance, ratchets, conditions, nudges and locks, and a melodic mode with accent and slide. |
| [4-generator-clips.html](4-generator-clips.html) | Generator clips: Euclidean rhythm, a chance melody that drifts from pass to pass, an arpeggiator. |
| [5-track-setup.html](5-track-setup.html) | A track's source and destination, echo while armed, MIDI 2.0 or MIDI 1.0 per destination, start-up messages, a missing device. The launcher is hidden here. |
| [6-clock-sync.html](6-clock-sync.html) | Internal tempo or another device's clock, sending clock to several devices with offsets, MIDI 2.0 tempo messages, MIDI Time Code out. |
| [7-import-export.html](7-import-export.html) | Importing a Standard MIDI File and a MIDI Monitor capture, pasting from MIDI Monitor, exporting MIDI 2.0 clip files or a Standard MIDI File. |
| [8-provenance.html](8-provenance.html) | About this project: the same provenance block as MIDI Glass and MIDI Patchbay, plus where each clip came from. |

## 3. How a project is organized

**A project** has a Tempo and meter track, any number of tracks and folders, a set of scenes (the launcher's columns), and the clips those refer to.

**A track** has a name, a color, a source (where it records from), a destination (where it plays to), M, S and R buttons (mute, solo, record arm), automation lanes and a short list of start-up messages. One destination per track keeps the model simple. To play the same part on two synths, duplicate the track: the clips stay linked, so an edit reaches both.

**Folders** hold tracks and other folders. M and S on a folder apply to everything in it. A closed folder draws an outline of what its tracks play, so the song's shape is still visible.

**Pinned rows** stay at the top of the track list, the launcher and the timeline while everything else scrolls, like frozen rows in Excel. Any track or folder can be pinned. The Tempo and meter track is pinned by default. A pinned track still belongs to its folder (the folder's M and S still apply). It's just drawn in the pinned area, with the folder's name in front of it.

**Clips.** A clip is notes, a pattern or a generator, with a length and a loop. Clips sit in launcher slots and on the timeline, and **a clip can appear in many places: it's the same clip each time.** Edit it once and every placement changes. Make a copy to break the link. The timeline shows a link mark and a count. I chose linked by default because it's what makes patterns and loops practical: a beat used forty times is one beat, not forty.

**Launcher or timeline, per track.** A track follows the timeline until you launch a clip on it. Then it plays that clip until you stop it or choose Back to timeline (per track, or for every track from the transport). This is how Bitwig Studio and Ableton Live handle the same choice, and it means the launcher and the timeline never fight over a track. Launching is quantized (next bar by default), so a clip starts in time.

**Scenes** are the launcher's columns. Launching a scene launches every clip in it. An empty slot stops its track when the scene launches, unless you take the stop button out of that slot.

**The launcher can be hidden.** Then the timeline gets the width (comp 5). Someone who only arranges never has to see it.

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

**Generator clips (comp 4)** in the first version:

- **Euclidean rhythm.** Rings of pulses spread as evenly as possible over a number of steps, each with a rotation, note, velocity and chance.
- **Chance melody.** Notes from a scale within a range, with how busy, note lengths, velocity range, and **Keep from the last pass**: 100% repeats, 0% is new every time, and anything between drifts slowly. That last setting covers the "evolving loop" idea without a separate type.
- **Arpeggiator.** Plays what you hold on the track's source, or another track's notes, in the usual orders, rates and octaves, with latch. While an arpeggiator clip plays, it listens to the track's source whether or not the track is armed.

Later candidates, in the order I'd look at them: per-lane speed and a fill mode for patterns; polyrhythm and "bouncing ball" generators; a generator that learns note-to-note movement from a clip you give it.

**The rule that makes all of this work: a result depends only on the clip's seed and the pass number.** Chance, conditions and generators are worked out from a random sequence seeded by the clip's seed (saved in the project) plus which pass this is. So playing from the start always gives the same hits, jumping back replays what you heard, and export writes exactly that. "Same" uses the same seed every pass. "Changes" mixes in the pass number. Keep from the last pass needs the previous pass, which the engine works out by running the earlier passes forward. That's cheap: it's a few hundred numbers, not sound.

## 5. MIDI 2.0 first

**What a clip stores.** Notes go into a note list at MIDI 2.0 resolution: start, length, note number, channel, 16-bit velocity, release velocity, attribute type and value, per-note pitch bend and per-note controller curves, and chance. Everything else is stored as the Universal MIDI Packet (UMP) message that was recorded or drawn: 32-bit controllers, registered and assignable controllers (RPN and NRPN), program change with bank, pitch bend, pressure, system exclusive, and Flex Data. The group is not stored. It belongs to the destination.

**MIDI 1.0 input.** A MIDI 1.0 device's notes are paired into the note list with their velocities scaled up using the MIDI 2.0 bit scaling spec (M2-115). That spec says scaling a value down after scaling it up gives back the original, so a part recorded from a MIDI 1.0 keyboard plays back to a MIDI 1.0 synth exactly as played. The spec only promises that when one implementation does both directions, which is one reason the app does its own conversion (next paragraph). Other MIDI 1.0 messages are kept as they arrived.

**What goes on the wire is decided per destination.** For each destination (device and group), the app reads the function block, or the group terminal block when that's all there is, and sends MIDI 2.0 or MIDI 1.0 to match. It converts in both directions itself. Windows only downscales for some endpoints, never because of what a function block declares, and it doesn't scale MIDI 1.0 up to MIDI 2.0 at all ([data translation](../../../../docs/kb/data-translation.md)). A per-track setting can force one protocol for a device that describes itself wrongly.

**What MIDI 1.0 can't say.** Per-note pitch bend, per-note controllers and per-note management have no MIDI 1.0 equivalent (UMP spec, appendix D.2.8), and a note attribute such as exact pitch has nowhere to go in a MIDI 1.0 note. A MIDI 1.0 destination doesn't get them. The app says so before it happens: a bar in the clip editor (comp 2), and a count in the export dialog (comp 7). Translating per-note expression to MPE for MIDI 1.0 synths that support it is a reasonable later option. The UMP spec allows for alternate translations like that, but MPE takes over channels, so it has to be a choice.

**Flex Data.** Tempo, time signature, key signature, chord names, lyrics and text live on the Tempo and meter track and in clips, are written to MIDI 2.0 clip files, and can be sent live. Devices that speak MIDI 2.0 can get Set Tempo messages next to MIDI clock (comp 6).

**MIDI-CI.** Program names come from the device over MIDI-CI Property Exchange when it offers a program list. The API already has this (`MidiCapabilityInquirySession.GetProgramListAsync`). Controller names (the controller list resources in M2-117) would let an automation lane say "Cutoff" instead of "NRPN 7:54", but the API doesn't have them yet. That's an API gap to file when we get there. A MIDI 2.0 clip file can also ask for a profile to be on before it plays (section 11), which the sequencer can do on the destination later.

**Note names.** The comps use C3 = 60, like the SDK helper and MIDI Patchbay. MIDI Glass uses C4 = 60. The family should pick one default (with a setting), and I'd decide that once for all three apps.

## 6. Recording

- **Arm a track (R) and play.** What arrives on its source is recorded, filtered by group and channel and by kind of message. System exclusive is off by default, so a device's chatter doesn't fill clips.
- **Echo while armed.** What you play goes straight to the track's destination, through the same translation as playback, so you hear the sound you're recording for. One echo hop through the service is the latency cost.
- **Three places to record:** on the timeline at the playhead (with loop recording that layers or replaces), into an empty launcher slot (it records for the launch length, then loops straight away), or **Capture**: every armed track keeps the last few minutes of what you played, and Capture turns it into a clip, even if you never pressed record.
- **Timing.** Windows timestamps each incoming message when it arrives. The app converts that to a bar and beat with the transport's start time and the tempo map. The device's own delay isn't known, so each source gets an optional "recorded late by" offset.
- Several tracks can record from one source (to layer a part on two synths), and the app opens one connection per source device and shares it.
- Count-in and punch in and out come in a later phase.

## 7. Automation

- **Drawn on the track itself** (comp 1): lanes open under a track on the timeline. A lane can be a controller (7-bit or 32-bit), pitch bend, channel pressure, program change, or a registered or assignable controller (RPN or NRPN). Tempo is a lane on the Tempo and meter track.
- **Clips have lanes too.** Controller lanes inside a notes clip travel with the clip and loop with it, which is what a launched clip needs.
- **Which one wins.** While a track plays a launched clip, the clip's lanes apply and the track's automation waits. On the timeline, a clip's lane for a controller wins over the track's lane for the same controller, but only inside that clip. The innermost lane always wins, so it's predictable.
- **How often values are sent.** A ramp is sent as steps, at most every 10 ms per lane, and only when the value changes at the destination's resolution. A 7-bit lane can't send more than 128 different values in a ramp, and a MIDI 1.0 cable carries only about a thousand controller messages a second, so a dense 32-bit lane going to a MIDI 1.0 device is thinned on purpose.
- Drawing: pencil, line, and curve handles, with optional snapping. LFO shapes from MIDI Glass and MIDI Patchbay (the shared LFO wave code) can fill a selection.

## 8. Clock, sync and timing

**How playback works.** Like the MIDI Player, the app hands each message to the service with the time it's due, and the service sends it then. The engine looks a short time ahead. The catch is that **a message handed over can't be taken back**, so a note you delete, a track you mute or a clip you launch inside that window still plays once. The Player looks 250 ms ahead. An interactive sequencer needs to react faster, so I'd start at 100 ms and measure in phase 0. Launches quantized to a bar are exact as long as the bar is further away than that. A launch set to Immediately starts one window from now.

**Silence** uses the Player's rules, which were measured: note offs for what's sounding, then sustain off before all notes off, then all sound off and pitch bend center, only on the channels the track uses (Windows MIDI Services is multi-client, so the app must not silence other apps), sent at once and again after the look-ahead. Starting in the middle of a song first sends each channel's bank, program, controllers and pitch bend, like the Player's seek.

**Internal clock.** The Tempo and meter track is the clock: tempo changes and ramps, and time signatures.

**Sending clock** (comp 6). MIDI clock pulses are scheduled from the tempo map like any other message, so a ramp and the notes stay locked together. The shared clock generator assumes one tempo at a time, so it isn't the right tool here. Start, Continue and Stop go out with the transport, and Song Position Pointer goes out when you move the playhead. Each device gets its own offset, the same idea as MIDI Clock. MIDI Time Code out reuses the shared time code generator, because time code doesn't depend on tempo.

**Following another device's clock** reuses the clock follower MIDI Patchbay's LFO uses: it turns pulses into a position and a tempo, and handles Start, Continue, Stop and Song Position Pointer. The honest limit is in the comp: the app can only plan as far ahead as the next pulse it expects, so following is looser than the internal clock. When you can, let the sequencer be the clock. If the clock stops arriving, playback stops (or, by choice, carries on at the last tempo).

**Not planned: following incoming MIDI Time Code.** MIDI Clock made the same call: it takes a phase-locked receiver and a way to bend the service's clock.

## 9. Files

**The project file is JSON** (`.midiproject`, in `Documents\MIDI Projects`; both names are open). Reasons: it matches MIDI Glass and MIDI Patchbay, so an assistant can read and write it with an agent guide like theirs; differences between two versions are readable; and the provenance block is shared code. MIDI is small. A dense hour of recording is a few megabytes. If measurement says a recording-heavy project parses too slowly, a clip's data can be stored as the bytes of a MIDI 2.0 clip file inside the same JSON, and the file stays one file. I'd only do that after measuring.

A sketch of the shape (not final):

```json
{
  "fileVersion": 1,
  "provenance": { "author": "Pete Brown", "license": "CC-BY-4.0", "digitalSourceType": "composite" },
  "ticksPerQuarterNote": 960,
  "tempo": [ { "tick": 0, "bpm": 124 }, { "tick": 61440, "bpm": 124, "rampTo": 128, "rampTicks": 7680 } ],
  "meter": [ { "tick": 0, "numerator": 4, "denominator": 4 } ],
  "tracks": [
    {
      "id": "t-bass", "name": "Bass", "color": "#5BC0EB", "folder": "f-synths", "pinned": false,
      "source": { "endpoint": { "name": "Keystep 37" }, "group": "any", "channels": "any" },
      "destination": { "endpoint": { "name": "Moog One" }, "group": 0, "channel": 1, "protocol": "automatic" },
      "timeline": [ { "clip": "c-bass-a", "tick": 15360 }, { "clip": "c-bass-a", "tick": 30720 } ],
      "slots": [ null, "c-bass-a", "c-bass-b", "c-bass-a", null ]
    }
  ],
  "clips": [
    {
      "id": "c-bass-a", "kind": "notes", "name": "Bass line A", "lengthTicks": 15360, "loop": true,
      "notes": [ [0, 403, 1, 40, 65535], [480, 403, 1, 40, 36044] ],
      "events": [ { "tick": 0, "ump": ["0x40B14A00", "0x80000000"] } ]
    },
    { "id": "c-beat-a", "kind": "pattern", "steps": 16, "stepTicks": 240, "seed": "91C3", "lanes": [] },
    { "id": "c-wander", "kind": "generator", "type": "chanceMelody", "seed": "4F2A", "keepPercent": 75 }
  ]
}
```

Groups and channels in the file count from 0, like MIDI Glass and MIDI Patchbay files. People and screens count from 1. A note is `[tick, length, channel, note, velocity]`, with optional fields after those only when a note has them (release velocity, attribute, per-note curves, chance), which keeps a big recording small. Other messages keep their UMP words; the group nibble is ignored, because the destination decides the group.

**Rules carried over from the other tools:** a file from an older version is converted with the original copied to an Earlier versions folder (MIDI Patchbay's rule). A file from a newer version is opened read-only rather than saved over, so newer settings aren't lost (the guard MIDI Patchbay and MIDI Glass got in October). A project or MIDI file can arrive by double-click from the internet, so every count and length in it is capped and checked, the way the SMF reader already does it.

**Undo.** MIDI Patchbay keeps a whole copy of the patch per undo step. That won't scale to a project with a hundred thousand notes, so the sequencer needs undo by change (each edit records how to reverse itself). That's a phase 1 design job.

## 10. Import

- **Standard MIDI Files** (.mid, .midi, .kar, .rmi, .smf) through the same reader as `MidiStandardFileReader`: formats 0, 1 and 2, karaoke text, chord symbols, and its limits for hostile files. Each file track becomes a clip on a new track (or clips on the selected track). The file's tempo is used when the project has none. Its program, volume and pan changes can become start-up messages. Its copyright text stays with the clips it came from.
- **MIDI 2.0 clip files** (.midi2). This needs a reader we don't have yet. It's the same reader the MIDI Player is waiting for, and both are waiting on reference files from another implementation to prove they read what others write.
- **Captures from MIDI Monitor and the console.** A CSV capture (MIDI Monitor's Save as CSV, or the console's `endpoint monitor --capture-format csv`) has exact times and every UMP message, so a jam you captured plays back exactly as it happened, one track per group. A .mid capture is MIDI 1.0 at 960 ticks per beat. Text captures with UMP words, and MIDI Monitor's .bin files, have no times, so they're laid out one message per step, which suits a set-up dump.
- **Paste.** UMP words copied in MIDI Monitor paste into a clip (comp 7).
- A small later change to MIDI Monitor could add "Open in Windows MIDI Sequencer" for a selection.

## 11. Export

**MIDI 2.0 clip files** (the MIDI Association's MIDI Clip File, M2-116-U, the format MIDI 2.0 calls SMF2). From the spec: the extension is `.midi2`; the file starts with `SMF2CLIP`; everything after that is UMP, big endian; any Set Profile On messages come first, then the ticks per quarter note message (the sequencer writes 960); every message after the profiles has a delta clockstamp in front of it; the configuration header may hold one Set Tempo, one Set Time Signature and set-up messages such as bank, program, volume and pan (our start-up messages); the sequence starts with Start of Clip and ends with End of Clip. Tempo changes in a standard MIDI file may only fall where a MIDI clock would (every 1/24 of a quarter note, per the UMP spec), so the exporter puts them there. Project name, clip name, composer and copyright go in as the standard Flex Data text messages.

**Whole project.** The MIDI Container File, the format meant to hold several clip files as tracks, isn't published: the MIDI Association's clip file page still says it "will have" one. Until it is, the choices are one clip file per track in a folder (my default), or one clip file with every track on its own group and channel (which loses the track structure and fails when two tracks share a channel). When the container format is published, whole-project export should move to it.

**Standard MIDI Files** (.mid, format 1, one track per track) through `MidiStandardFileWriter`, which already turns MIDI 2.0 into MIDI 1.0 by the rules in the data translation article and counts what it had to leave out. The export dialog shows that count before you export (comp 7).

**Patterns, generators and notes with chance** are written the way they play from bar 1, with their saved seeds. A file holds notes, not the settings that made them.

## 12. Provenance

**The project carries the same provenance block as MIDI Glass and MIDI Patchbay** (the shared `ContentProvenance` code): id, version, author, organization, web page, license (SPDX), created, the tool, how it was made (`digitalSourceType`) and the AI disclosure. New projects fill author and license from the same author profile the other tools use.

**Each clip remembers how it began** (comp 8). That's what lets the app suggest how the whole project was made, and it's what you need before you share or sell a piece that uses someone else's file.

| A clip that was | Is described as (IPTC digital source type) |
| --- | --- |
| Recorded from a performance | `digitalCapture` |
| Drawn, typed in, or set step by step in a pattern | `digitalCreation` |
| Made by a generator | `algorithmicMedia` |
| Imported from a file | kept with its `basedOn` source: the file name and the file's own copyright text |
| Written by an AI assistant | `trainedAlgorithmicMedia`, with the AI disclosure (who checked it, and the model when known) |

The project as a whole is usually a mix, which IPTC calls `composite`, or `compositeSynthetic` when at least one part was made with generative AI. The app suggests one, and you choose. **One shared-code change:** the shared provenance code only recognizes five terms today. It needs `digitalCapture` and `composite` added so they show as plain words. Unknown terms already round-trip, so this is a display change, not a format change.

**What reaches an exported file:** a MIDI 2.0 clip file has standard messages for the project name, clip name, composer and copyright, and a Standard MIDI File has text events for the same. Neither has room for the rest, so the full record stays in the project file. Signed packs for sharing projects (like MIDI Glass's) can come later and reuse the same pack code.

## 13. What it reuses

**Can it reuse the Sequencing WinRT API?** For files and the model, yes. For playback, no.

- **Use:** the Standard MIDI File reader and writer, the sequence model's tempo map, time signature map and bar and beat maths, and `MidiSequenceBuilder` for anything that hands a sequence to other code. In-repo tools compile these shared native sources directly, the way the MIDI Player does, so the app pays no cost for crossing the API boundary on hot paths. This is `src/in-box/Inc/midi_file_sequence.*`, `midi_file_smf_reader.*` and `midi_file_smf_writer.*`.
- **Don't use `MidiSequencePlayer` as the engine.** It plays one linear sequence with a fixed look-ahead. It has no loops, no launching, no edits while playing, no recording and no external clock. MIDI Glass's design already argued against adding a live mode to it, and I agree: a second way of working would make a stable, tested player worse. Instead, the sequencer gets its own engine, built on the Player's measured rules for scheduling, silence and chasing state (section 8).
- **API that could follow, after this release's API lock:** a MIDI 2.0 clip file reader and writer in `Windows.Devices.Midi2.Utilities.Files`, with this app as the first user and the MIDI Player as the second. That's the same way the Player shaped the file reader. A public live-sequencing engine is not something I'd offer until third parties ask for one.

From `midi-app-shared`: the endpoint catalog and endpoint matching (device presence, missing devices), service status, window chrome, settings, the appearance flyout, single instance and file handoff, provenance and author profile, the time code generator, the clock follower, LFO shapes, the channel voice message builders, and later the AI assistant prompt and the pack code. From the MIDI Player: the Composition drawing approach for notes and grids (a pool of visuals sized to what's on screen) and its General MIDI names.

## 14. How it fits with the other tools

- **MIDI Monitor and the console:** their captures import, and MIDI Monitor's copied messages paste (section 10).
- **MIDI Clock:** can be the master clock for the sequencer and your hardware through a loopback, or the sequencer can be the clock (comp 6).
- **MIDI Patchbay:** play a track to a loopback and use Patchbay's steps in between. Later, its transform steps could become track effects inside the sequencer.
- **MIDI Glass:** a touch surface for the sequencer once remote control exists (phase 8): launch clips and scenes, run the transport, toggle steps.
- **MIDI Player:** plays what the sequencer exports, and shares its reader and drawing approach.
- **MIDI Settings:** not on its toolbar. You said the toolbar is for utilities and setup, and the Player and Clock aren't there either.
- **Loopback Setup:** the destination list can make a loopback without leaving the app (comp 5).
- **AI assistants:** an agent guide for project files, like the MIDI Glass and MIDI Patchbay guides, is a later phase. The file format choices in section 9 are partly for this.

## 15. Accessibility and keyboard

- Every grid has a keyboard path and a screen reader name: a launcher cell reads like "Bass line A, notes clip, scene 2, stopped", and a step reads like "Step 7, Closed hi-hat, on, velocity 73 percent, ratchet 3".
- No state is shown by color alone: M, S and R are letters, a playing clip has a triangle and an outline, a queued clip says which bar it starts on, and a recording slot says Recording.
- The piano roll can be used without a mouse: arrow keys move between notes, a cursor adds notes, and every value can be typed in the inspector.
- The family's existing rules apply (toggle state changes rather than clicks, names on template roots, the accessibility checker script).

## 16. Performance

- Notes, steps, automation and the timeline are drawn with Composition from pools of visuals, so the cost follows what's on screen, not the size of the project. The MIDI Player does this today.
- The engine runs on its own thread. The UI reads snapshots and never takes the engine's lock on a frame. In the MIDI Player, asking the engine from the frame code would have meant about 500 lock waits a second competing with playback, so it keeps its own copy for drawing.
- Targets to measure in phase 0: a 100,000-note project scrolls smoothly, 64 tracks by 16 scenes, and 16 tracks of dense automation going to MIDI 1.0 devices.

## 17. Where it lives and what it's called

| | Proposal |
| --- | --- |
| Name | Windows MIDI Sequencer. Start menu shortcut: MIDI Sequencer. |
| Executable | `midisequencer.exe` |
| Source | `src/in-box/user-tools/midi-sequencer/`, in the Tools installer like MIDI Glass and MIDI Patchbay. The other choice is `src/standalone-apps/` with its own installer, like the SoundFont Synth. |
| Settings | `HKCU\Software\Microsoft\Windows MIDI Services\Tools\midisequencer` |
| Projects | `.midiproject` files in `Documents\MIDI Projects` |
| MIDI session name | The project name, so other tools can show who is using a device |

## 18. Phases

Each phase ends with something you can use.

| Phase | What lands |
| --- | --- |
| 0 | Spikes and measurements: look-ahead against edit and launch latency, following a clock from MIDI Clock through a loopback, drawing 100,000 notes, JSON size and load time. |
| 1 | The project model and file, undo by change, Standard MIDI File import and export, the MIDI 2.0 clip file writer and reader. A test project like MIDI Patchbay's and MIDI Glass's. No window needed. |
| 2 | The engine: timeline playback to many destinations, protocol per destination, mute, solo, silence, chase, internal clock, clock out. |
| 3 | The main window: tracks, folders, pins, the timeline, the piano roll, recording with echo. |
| 4 | The clip launcher: scenes, quantized launching, recording into slots, Capture. |
| 5 | Automation lanes and tempo map editing. |
| 6 | Pattern clips. |
| 7 | Generator clips. |
| 8 | Following another clock, MIDI Time Code out, MIDI remote control (for MIDI Glass and hardware controllers). |
| 9 | Provenance screens, capture import and paste, docs, an agent guide, an accessibility pass, the installer. |

## 19. Risks and unknowns

- **Look-ahead against responsiveness.** 100 ms is a guess until phase 0 measures it.
- **Following a clock.** How loose it is with real devices, and how much the clock follower's smoothing helps, is unmeasured.
- **MIDI 2.0 clip file interoperability.** We can write files that match the spec, but we can't prove other software reads them until we have files from another implementation.
- **The MIDI Container File.** Whole-project export waits on a spec with no date.
- **Undo by change** is new work for this family and is easy to get subtly wrong.
- **Large recordings in JSON** might be slow to load. Measure before changing anything.
- **Demand for MPE output.** I don't know how many people would use it. It's a later phase either way.

## 20. Decisions I need from you

1. **Step sequencers and generators as clip kinds in this app**, rather than a separate app? (Section 4.)
2. **Tools installer** (`src/in-box/user-tools`) or **standalone** (`src/standalone-apps`)?
3. **Names:** Windows MIDI Sequencer, `midisequencer.exe`, `.midiproject`, `Documents\MIDI Projects`?
4. **Linked clips by default**, with Make a copy to break the link?
5. **JSON project file**, accepting larger files for heavy recordings, so assistants and people can read it?
6. **Whole-project MIDI 2.0 export as one clip file per track** until the MIDI Container File is published. Do you know when that's expected?
7. **Note names:** C3 = 60 or C4 = 60 as the family default, and should MIDI Glass change to match?
8. **Look-ahead:** start at 100 ms and let phase 0 decide?
9. **A metronome:** with no audio, it would be notes sent to a destination you choose (for example the General MIDI Synth). Wanted in the first version?
10. **Shared provenance change:** add `digitalCapture` and `composite` to the terms the shared code knows?
