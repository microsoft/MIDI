# MIDI Glass: row grids, grid buttons, and a fretboard (proposal, Oct 24 2026)

The comps are `33-pad-grid-rows.html`, `34-pad-grid-buttons.html` and `35-fretboard.html` in this folder, with their pictures in `shots\` under the same names.

## What already exists (so we don't build it twice)

- Note pads and hex pads have a key, a scale, "Only the key" mode (Push-style remap), next-pad and next-row steps in semitones or scale steps, glide (portamento or MIDI 2.0 per-note bend), and colors for root / in key / out of key.
- A Switch can already be a root picker or a scale picker. The customer's screenshot is exactly that: an 8 x 8 in-key grid with both pickers.
- Two pads playing the same note already share one voice (`binding/PadVoices.h`): the note starts with the first finger and ends with the last.
- Limits today: `MaximumPadCount = 128`, and pads flow into as many rows as the control's width holds. So there's no way to say "8 rows of 24".

## 1. Fixed rows or columns, past 128 pads

Recommendation: extend Note pads and Hex pads with one new setting, not a new control kind.

- `Arrangement`: **Flow** (today, still the default, so every saved layout opens unchanged), **Rows**, **Columns**.
- Rows and Columns take `Rows` (1-32) and `Pads in each row` (1-64), 1,024 pads at most. Pads stretch to fill the control, so resizing changes pad size, never the shape.
- Rows: pads run left to right, rows stack bottom to top. Columns: pads run bottom to top, columns stack left to right; "next pad" means up and "next row" means the next column right. Hex pads in Columns draw flat side up.
- Row offset is the existing next-row step (semitones, or scale steps in "Only the key"). Presets: Fourths, Thirds, Sequential (carry on from the row below), Octave, Custom.
- Pads whose note falls outside 0-127 draw blank and play nothing (the existing -1 rule).
- New option "Light every pad playing a held note", on by default for Rows/Columns. Push and LinnStrument both do this; with repeated notes it's how you see the shape you're holding.
- Everything else comes along unchanged: pickers, grid buttons (below), glide, colors.

Why not a new kind: in-key math, pickers, voices, glide, colors, inspector and serializer all key off `IsPadGrid`. A new kind would copy all of it, and the Oct 13 notes put the cost of one new kind at about 20 code paths.

JSON: `pads.arrangement` (`flow`/`rows`/`columns`), `pads.rows`, `pads.padsPerRow`. Written only when not `flow`, so old builds keep reading files from old layouts.

Risks to measure before raising the cap: draw and hit-test time at 1,024 pads on a 4K screen, and the narrator's tree (1,024 pad items is a lot; one item per row with a value pattern may be better).

## 2. Grid buttons on the canvas (controls that change the pads, not MIDI)

I'm reading "not just in the menu" as "not only in the editor's inspector": today the only runtime change is the two pickers. If the customer meant something else, the design below still covers it.

Recommendation: keep the pattern Pete chose on Oct 13 (ordinary controls that change the pads, no new control kinds), and widen it three ways.

**a. A new message kind, `ChangePads`, for Buttons and Pads.** Same family as `GoToPage`: it runs from the action plan on press, sends nothing, and needs no device. A row says which change and which pads:

- Octave up / down; Row up / down (shift the whole grid by one row step, Push's arrow keys); Pad left / right; Semitone up / down.
- Every note; Only the key; Switch between the two.
- Next / previous root; next / previous scale; next row step.
- Reset to saved.

**b. Two more things a Switch (or Toggle) can pick.** `Notes` (Every note | Only the key) and `Next row` (a list of steps); maybe `Octave` (-2 to +2). A Toggle that picks Notes lights when the pads play only the key, which answers "an easy way to switch chromatic and key-only at runtime".

**c. A Readout that shows the pads' state.** `Shows: The pads' key` (or octave, row step, all). Updates whenever anything changes the pads.

**Which pads.** Default is every pad grid, matching today's pickers ("one key per layout", by design). New: a row can name one grid instead. That's the first thing a split layout (bass pads left, lead pads right) asks for, and octave shift per grid is the obvious case. **Decision for Pete:** allow per-grid targets for octave/row shifts only, or for key and scale too (which ends "one key per layout").

**Making it easy.** Next to today's "Add pickers" button on the pad grid's Key section: "Add grid buttons...", a short checklist (octave, row, notes toggle, readout, reset) that drops ready-wired controls beside the grid. Palette entries stay out, as Pete decided Oct 13.

Runtime state (picked key, octave offset, row shift, notes mode) lives with the existing `PickedKey` in `LivePlayer`, never saved, and Try mode forgets it on exit like it does today. Held notes keep sounding through a change, as now.

## 3. Fretboard

Recommendation: a new control kind, `Fretboard`. It reuses the pad grid math (string = row, fret = column) but draws and plays differently enough that hiding it inside Note pads would make both inspectors confusing.

**What you set**

- Tuning presets: guitar Standard, Drop D, Half step down, Whole step down, DADGAD, Open G, Open D, Open E, 7-string; bass 4 and 5 string; ukulele (G C E A, high G), baritone ukulele, mandolin, banjo open G (short 5th string that starts at fret 5). Custom: each string is a note box; editing one makes the tuning "Custom". 1-12 strings.
- Frets on the neck (default 22), frets showing (for example 0-12 or 5-9). A neck strip shows where the window sits and drags it, the way GarageBand moves along the neck.
- Lowest string at top (default, how you see your own guitar, and what GarageBand does) or bottom (tab view). Left-handed mirror.
- Key guide: dots on in-key frets, root in its own color. Out-of-key frets: Play (default; a guitar is chromatic), Dim, or Don't play. No "Only the key" remap here: on a fretboard that would break every chord shape a guitarist knows.
- Capo, and the window position, can also be changed at run time by `ChangePads` buttons (Capo up/down, Neck left/right) and a Switch that picks Tuning.

**How it plays**

- One note per string. Fretting higher on a string replaces the lower note (hammer-on); lifting goes back to the fret still held (pull-off). With hammer-on/pull-off on, the new note is sent legato (note on before the old note off) so a mono synth slurs.
- Bend: moving the finger across the string bends the note up (either direction, like a real string), up to a set range (default 2 semitones). Wiggling does vibrato for free.
- Slide along the string: Fret by fret (bend in semitone steps, no new attack), Smooth (fretless), or Off (new note at each fret). This is the existing `PitchAtPoint` "only the row decides" rule, which already fits strings.
- Notes ring: While held, or Until the string plays again (a real string keeps ringing after you lift, until you fret it again or mute it). A Mute strings button ends them.
- Play mode: **Touch a fret** (default; touching plays, GarageBand's Notes view) or **Fret, then strum**: fretting is silent and a swipe across the strum zone plays each string at its fretted note (open if nothing is fretted), low to high on a down swipe. Swipe speed sets velocity and the time between strings.

**What it sends (MIDI only, so the synth does the sound)**

- Per-note bend (MIDI 2.0): same as pads today; each string bends on its own.
- Channel per string (MIDI 1.0): string 1 on one channel, string 2 on the next, each mono, pitch bend per channel, bend range set with RPN 0 before the first note. MIDI 1.0 spec 4.2 says Mono mode is "particularly useful for receiving MIDI data from guitar controllers" and for "independent pitch bending of individual notes". This is how hardware MIDI guitar pickups work, and most guitar sample libraries expect it.
- MPE: the MPE profile (M2-120 v2.0.3). One manager channel plus a member channel per string. Section 3.5: pitch bend sensitivity is the same on manager and member channels, sent only on the manager channel, default 48 semitones (HCUs). Third dimension is CC 74.
- Per-string channels also fix the one thing shared voices get wrong for guitar: the same pitch on two strings (unison bends) must be two notes.

Honest limits: most Windows touch screens report no pressure, so velocity comes from the existing soft-hit setting, swipe speed, or a pen. Palm mute, harmonics and slides into a note would need articulations (the Note-On Orchestral Articulation profile, M2-123, is worth a look later; I haven't read it for this).

**Later, not in the first round:** GarageBand-style chord strips (one strip per chord in the key, tap a string or strum), strum patterns, 12-string pairs.

Prior art I drew from: GarageBand for iOS (Smart Guitar notes view, neck strip, bend by pushing across the string, chord strips), Ableton Push (In Key / Chromatic, layout and octave buttons, duplicate pads lit), Novation Launchpad scale mode, LinnStrument (row offset setting, one channel per row for guitar-style play), and MIDI guitar pickups (channel per string).

## Pete's decisions (Oct 24 2026)

1. Extend Note pads and Hex pads (no new row-grid kind).
2. Grid buttons support BOTH: a list of target grids, or every grid on the layout. Per-grid key and scale are allowed, so "one key per layout" ends when a button or picker targets specific grids.
3. Pad cap 1,024.
4. Fretboard strings are a list of strings with properties (note, and later more); the user can reorder them. Default: low string at the BOTTOM.
5. Round 1 = all the non-fretboard work (sections 1 and 2). Round 2 = all fretted-instrument work, including strumming. The strum area can be on the left or right side (replaces "Left-handed").
6. Comps and this proposal live in `src\prototypes\midi-glass\design\` in the repo.
7. Labels follow the Oct 23 label rules (below).

## Labels (Oct 23 rules: one domain term or noun, commands drop articles, empty values in parentheses)

Matches existing pad labels: "Row offset", "In key only", "Root note", "Scale", "Function" (the Switch picks combo).

| Where | Label | Values |
| --- | --- | --- |
| Pads | Arrangement | Flow, Rows, Columns |
| Pads | Rows / Columns | number |
| Pads | Pads per row / Pads per column | number |
| Pads | Row offset (existing) | Fourths, Thirds, Sequential, Octave, Custom |
| Pads | Highlight held notes | check box |
| Message row kind | Pad grid | (row sends no MIDI) |
| Pad grid row | Function | Octave up, Octave down, Row up, Row down, Shift left, Shift right, Semitone up, Semitone down, In key only on, In key only off, In key only toggle, Next root, Previous root, Next scale, Previous scale, Next row offset, Reset |
| Pad grid row, Switch picker | Targets | All grids, or a list of grids by name; empty list shows "(No grids)" |
| Switch Function (existing combo) | adds | In key only, Row offset, Octave |
| Readout | Source | adds Pad grid key, Octave, Row offset, Pad grid summary |
| Pad grid inspector | Add grid buttons (command) | next to "Add root and scale pickers" |
| Fretboard (round 2) | Tuning | Standard, Drop D, Half step down, Whole step down, DADGAD, Open G, Open D, Open E, 7-string, Bass, 5-string bass, Ukulele, Baritone ukulele, Mandolin, Banjo, Custom |
| Fretboard | Strings | list; each string: Note; Move up / Move down / Remove / Add string |
| Fretboard | Frets | number on the neck |
| Fretboard | Visible frets | from / to |
| Fretboard | Capo | number |
| Fretboard | Strum side | Left, Right, (None) |
| Fretboard | Play mode | Tap, Strum |
| Fretboard | Sustain | While held, Until replayed |
| Fretboard | Scale dots | root + scale, or (None) |
| Fretboard | Out-of-key frets | Play, Dim, Mute |
| Fretboard | Bend range (semitones) | number (matches pads) |
| Fretboard | Slide | Stepped, Smooth, Off |
| Fretboard | Hammer-ons | check box |
| Fretboard | Output | Per-note bend, Channel per string, MPE |
| Fretboard runtime | Mute (command) | |
| Fretboard | Visible frets | From, To (0 and 1 both start at the nut) |
| Fretboard | Scale dots | Root note ((None) + C to B), Scale |
| Fretboard | Expression (heading) | Bend range (semitones), Slide, Hammer-ons, Output |
| Fretboard | Add fretboard buttons (command) | adds a Tuning picker and Capo down, Capo readout, Capo up, Down the neck, Up the neck, Mute |
| Pad grid row | Function | adds Capo up, Capo down, Up the neck, Down the neck, Mute |
| Switch Function (existing combo) | adds | Tuning |
| Readout | Source | adds Capo, Tuning |

## Rounds

Round 1 (now): Arrangement Rows/Columns + 1,024 cap + Highlight held notes; Pad grid message kind with Targets; Switch Function additions; Readout sources; Add grid buttons; per-grid key/scale state at run time.

Round 2 (later): Fretboard control, all of section 3, strum side left or right, reorderable string list.

## Round 2 as built (Oct 24 2026)

Where the build differs from section 3, or where section 3 left a choice open:

- Strings are listed top to bottom as drawn, so standard tuning lists the high E first and the low E sits at the bottom (Pete's decision 4). Each string has a note, and a banjo's fifth string also has the fret it starts at; there's no box for that one, only the Banjo tuning sets it.
- Strum side Left turns the neck round, nut on the right, so the strum area is always on the body side. (None) has no strum area; Strum play mode then plays like Tap.
- The highest fret held on a string sounds. A lower fret pressed behind it plays nothing, the way a real string works.
- Sustain While held ends a fretted string when its finger lifts. A string strummed open has no finger, so it rings until it's played again or muted.
- Out-of-key Mute damps the string: tapping a muted fret on a ringing string stops it.
- A slide or a push across a string bends; the bend goes out against a fixed sensitivity rather than the bend range: 24 semitones on each string's channel (Channel per string), 48 on the manager channel (MPE, the profile's default), 48 for per-note bend. It's sent once per channel per run, before that channel's first note.
- MPE uses the note row's channel as the manager and the channels above it as members. Windows MIDI Glass doesn't turn the profile on with MIDI-CI, so the instrument has to be set up for MPE.
- Per-note bend output shares one channel, so two strings on the same note are one note (it starts with the first and ends with the last). Channel per string and MPE keep them apart.
- Runtime changes (capo, frets showing, tuning, the dots' root and scale) reuse the pad grid targets, functions, pickers and readouts, and like them are never saved. Root and scale functions and pickers move a fretboard's dots when it shows a key; octave, row and in-key functions leave fretboards alone.
- Add fretboard buttons adds all seven controls with no checklist: the tuning picker standing beside the fretboard, and the row under it.
- Not built: chord strips, strum patterns, 12-string pairs, articulations.