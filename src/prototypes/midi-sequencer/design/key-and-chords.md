# Key and chords for Windows MIDI Sequencer: proposal (revision 2)

Status: revised November 1 2026 with Pete's answers. Phase 1 (section 10) is being built: the row, saving, editing, and the key and chord pickers. Mockups: [key-and-chords-mocks.png](key-and-chords-mocks.png).

## The idea in one paragraph

An optional **Key and chords** row sits pinned under **Tempo and meter**. It holds key changes (a root and a scale) and chords (a root, a type and a bass note), each with a start and a length. People draw their own chords, ask for the next chord, fill a stretch with a well-known progression, or ask an AI assistant to write the chords and sections for a whole song. Generators and melodic patterns follow the song's key and chords unless they're set to their own key. The piano roll shows which notes are in the key and in the current chord. Chords can be turned into notes on a track. MIDI 2.0 devices receive the key and the chord names, the way they already receive tempo.

## Pete's decisions (November 1 2026)

1. **One row, two bands**, as long as chords move, stretch, copy and split exactly like clips. They do (section 1).
2. **New generator and melodic pattern clips follow the song's key** when the sequence has one.
3. **Roman numerals show by default** when they fit on the chord block, with a setting to hide them.
4. **Chord names are spelled in the key** (B♭ in F major, A♯ in B major) everywhere, including what's sent over MIDI 2.0.
5. **Play chords is wanted if it's simple.** It plays on a new **Default output** setting, so nothing has to be set up on the row itself (section 1).
6. **New from Pete:** **Ask an AI assistant…** on the row, which writes the song's key, chords and sections, can be turned off in Settings, and is documented in an article (section 7).
7. **New from Pete:** drag chords onto a track as a clip, and **Copy chords to new track**. The result is ordinary notes with no link back to the chords (section 6).

## Label rules used here

From the label passes of October 1 and 23 2026, applied to every label in this proposal and to be checked again in the build:

- Labels, headings, toggles and picker options are nouns or known music terms, in sentence case, never phrases. ("Chord tones", not "Lean on chord notes".)
- Commands drop articles and "this" or "here". ("Copy chords to new track", not "Copy all the chords to a new track".) A command that opens a dialog ends in "…".
- An empty value is in parentheses: "(None)".
- The AI command is **Ask an AI assistant…** and its setting is **AI assistant**, the same words Windows MIDI Glass and Windows MIDI Patchbay use.
- Status text and explanations are plain sentences, not labels, and those rules don't apply to them.

## 1. The row

- **Name:** **Key and chords**. A thin band on top shows each key ("A minor"), and chord blocks sit under it with the chord name ("Dm7") and, when it fits, the Roman numeral ("ii7").
- **Optional.** New sequences don't show it. **Add track** gets **Key and chords**, and the row's menu has **Hide**. Hiding keeps the chords. A sequence with chords opens with the row showing.
- **Chords work like clips.** Double-click the chord band to add one at the snap point. Drag to move, drag the right end to change the length, Ctrl-drag to copy, Shift-drag to keep it in time only, Ctrl-click to pick several, the razor splits one, and Delete removes them. Everything can be undone.
- **Key changes** work the same way in the key band: double-click to add, drag to move. Selecting one opens the key picker: **Root**, **Scale** (the 11 scales generators already use) and a **Transpose chords** toggle that moves the chords after it into the new key.
- **A chord from outside the key** has a star and a warm outline. It isn't an error, just worth seeing.
- **Placeholder block** after the last chord: **Add chord**, which opens the picker with suggestions.
- **Keyboard:** with the row focused, Left and Right move between chords, Enter opens the picker, and N adds the top suggestion after the selected chord.
- **Hearing chords:**
  - Hovering over a chord in the picker plays it.
  - **Play chords**, a toggle in the row's menu, plays the row's chords as held notes while the sequence plays.
  - Both play on **Default output** from Settings (section 8). The first time either is used with no default output set, the app picks the General MIDI Synth if it's there, and says so in the status bar. Otherwise it asks once.

## 2. Picking a chord

The picker opens from a chord block or **Add chord**. Sections, top to bottom:

- **Next chord.** A caption says what it's based on ("After Am (i) in A minor"), then four or five chords as large buttons, most likely first. Borrowed chords have a star.
- **More chords.** Borrowed chords (iv and ♭VII in a major key), a secondary dominant leading to the top suggestion (V7 of V), and a sus4 version of the suggested V.
- **Chord.** Build one by hand:
  - **Root**: all 12, spelled to suit the key.
  - **Type**: Major, Minor, 7, Maj7, m7, Dim, Sus4, Sus2, then **More**: 6, m6, 9, Maj9, m9, Add9, Aug, Dim7, m7♭5, 7sus4, 11, 13.
  - **Bass**: (Root), or any note, for chords like C/E.
- **Progression.** A list, a **Rate** list (2 per bar, 1 per bar, 1 per 2 bars) and **Fill**. It fills from the selected chord or the playhead for as many bars as the progression needs, in the current key:
  - Pop: I–V–vi–IV, vi–IV–I–V, I–vi–IV–V ("50s")
  - Jazz: ii–V–I, I–vi–ii–V
  - Blues: 12-bar blues, plain or all 7ths
  - Minor: i–VI–III–VII, i–iv–v, Andalusian (i–VII–VI–V)
  - Classical: Canon (I–V–vi–iii–IV–I–IV–V), Circle of fifths
  - Saved progressions (section 9)
- **How suggestions are picked.** Fixed music theory rules, not AI, so the same chord always gets the same suggestions:
  - Each chord in the key has a job: home (I, vi), moving away (ii, IV), and pulling back (V, vii°). The next chord is usually the next job along.
  - Ranked lists per scale degree. In a major key: after I, try IV, V, vi, ii; after ii, V, vii°, IV; after IV, V, I, ii; after V, I, vi, IV; after vi, IV, ii, V; after iii, vi, IV. Minor keys have their own list, which includes the major V7 that minor keys borrow.
  - On the last bar of a four or eight-bar phrase, V and I move up the list.
- **Suggest** on the row header fills empty bars up to the next chord with the top suggestion each time. One undo step.

## 3. What follows the key and chords

- **Key** on generators and melodic patterns gets a first option, **Song key**, before the 12 roots. New clips use **Song key** when the sequence has a key (Pete's decision 2), and their own key otherwise. Existing clips keep their own key.
- **Chance melody, Learn, Words, Evolve, Infinity series, L-system, Change ringing** follow the song's root and scale where the key changes. A **Chord tones** toggle makes strong beats pick from the chord playing at that moment; the rest come from the scale.
- **Arpeggiator:** a new **Source** option, **Chords**, beside the held keys and another track.
- **Melodic patterns** with **Song key** move each step by scale steps when the key changes (C in C major becomes D in D major; E in C major becomes E♭ in C minor).
- **Notes clips:** off by default. A **Follow key** toggle in the clip editor moves the clip's notes by scale steps when the key changes under it. Notes never change to fit chords.
- **Drum patterns, Euclidean rhythms and Life drums never follow.**
- A placement that follows the song's key shows a small key mark on the timeline, so it's clear why it plays different notes in different places.

## 4. Help while writing notes

- **Scale highlight** (toggle in the piano roll header, on by default): rows in the key are lighter, the rest darker, and the notes of the chord playing at each moment are tinted under that chord.
- **Scale snap** (toggle, off by default): drawn and dragged notes land on notes in the key. Hold Alt for any note.
- **Find key** (command on a notes clip or a track): suggests the most likely key from its notes, with the next two as choices, by matching how often each note is used against the typical shape of each key (Krumhansl and Schmuckler's key profiles). Applied only if picked.
- **Detect chords** (later): suggests a chord for each bar from a clip's notes.

## 5. Saving and sending

- **The sequence file** gets two new lists, `keys` and `chords`, each item with a tick and a length. Older versions keep them as unknown keys.
- **MIDI 2.0 devices** get UMP Flex Data **Set Key Signature** (status bank 0x00, status 0x05) and **Set Chord Name** (status 0x06: tonic, chord type, up to four alterations, and a bass note with its own type and two alterations), checked against M2-104-UM v1.1.2. A **Key and chords** toggle per device in **Studio setup**, beside the existing tempo toggle. Chord types not in the spec's list go as the closest type plus alterations. To check during the build: the chord type numbers, and how a mode such as Dorian is sent (the message carries sharps or flats and a tonic, not a mode).
- **Standard MIDI files:** export writes a key signature event (FF 59) at each key change. That event only knows major and minor, so a mode becomes the key signature it shares. Chords have no standard event; export can write them as text events, behind a toggle that's off by default. Import reads FF 59 into the row.

## 6. Chords to notes

- **Drag a chord, or several picked with Ctrl, onto a track.** Where they're dropped, they become one notes clip (Linear), named after the first and last chord ("Am – E7"). The original chords stay.
- **Copy chords to new track** (command on the row's menu): a new track with one notes clip covering every chord, on **Default output**.
- **Voicing:** each chord is held for its length, in close position around middle C, with the bass note an octave below. An **Inversion** picker on the drop could come later; the first version keeps it simple.
- After that it's plain MIDI. Changing a chord doesn't change the clip, and the clip shows no link mark.

## 7. Ask an AI assistant

- **Where:** **Ask an AI assistant…** on the row's menu and on the row header's menu. Hidden when **AI assistant** is off in Settings (on by default, as in Windows MIDI Glass).
- **What it does:** writes the key, the chords and the sections for a whole song. No clips, notes, drums or loops. The sections become tags on **Tempo and meter** (the existing tags, like "Verse" and "Chorus"), with one color per kind of section.
- **The dialog**, built on the shared `AssistantPrompt` from `midi-app-shared`, with a second step for the answer:
  1. **Prompt**: the starting prompt, with **Copy**, and a link to the guide. The note under it says the app sends nothing anywhere itself.
  2. **Answer**: a box for pasting the assistant's answer, and **Check**. Checking shows a summary ("8 sections, 64 bars, 31 chords, 1 key change") or says plainly what's wrong ("Chorus has 36 beats of chords but is 8 bars of 4/4, which is 32").
  3. **Apply** with **Replace** (the row's chords and keys, and the tags) or **Add at playhead**. One undo step.
- **The prompt tells the assistant to ask the person, one question at a time, and to offer a sensible default for each:**
  1. Style or genre, and the mood.
  2. Song structure (it suggests one for the style, such as Intro, Verse, Pre-chorus, Chorus, Verse, Chorus, Bridge, Chorus, Outro) and each section's length in bars.
  3. The key, or let the assistant choose. Any key changes.
  4. How rich the chords should be: Simple (triads), Sevenths, or Jazz (extensions and alterations).
  5. How often the chords change.
  The prompt also gives it the sequence's meter, tempo, current key and existing tags, so it doesn't ask what it can already read.
- **The answer format** is one fenced block marked `midi-sequencer-song`, holding JSON:

```json
{
  "format": "windows-midi-sequencer-song",
  "version": 1,
  "meter": "4/4",
  "key": { "root": "A", "scale": "naturalMinor" },
  "sections": [
    { "name": "Intro", "bars": 4, "chords": [ { "chord": "Am", "beats": 8 }, { "chord": "F", "beats": 8 } ] },
    { "name": "Chorus", "bars": 8, "key": { "root": "C", "scale": "major" },
      "chords": [ { "chord": "C", "beats": 4 }, { "chord": "G/B", "beats": 4 } ] }
  ]
}
```

  - **Chord grammar:** a root (A to G, with ♯, ♭, # or b), a type from the picker's list (nothing for major), and an optional "/" and bass note. "N.C." means no chord.
  - **Scales:** the 11 names the sequencer already uses: major, naturalMinor, dorian, phrygian, lydian, mixolydian, harmonicMinor, majorPentatonic, minorPentatonic, blues, chromatic.
  - **Rules the app checks:** each section's chord beats add up to its bars times the beats in a bar; at most 256 sections, 999 bars and 4,096 chords; names at most 40 characters. Anything else is refused with a plain reason, never half applied. Unknown extra keys are ignored.
- **The article:** `docs/kb/midi-sequencer-key-and-chords-for-agents.md`, in the same shape as the MIDI Glass and MIDI Patchbay agent guides: what to ask, the whole format with a working example, the checks the app makes, and a script that checks an answer before it's pasted. It's added to the table in `AGENTS.md`. The dialog's link is a placeholder until Pete has the aka.ms link (expected November 2 2026).

## 8. Settings

New entries in the Sequencer's Settings pane:

| Label | Kind | Default | What it does |
| --- | --- | --- | --- |
| Default output | Device picker | (None), then the General MIDI Synth on first use | Where chords are heard, and where **Copy chords to new track** sends its track. |
| Roman numerals | Toggle | On | Shows each chord's numeral on its block when it fits. |
| AI assistant | Toggle | On | Shows **Ask an AI assistant…** on the Key and chords row. |

## 9. Later

- Saved progressions: save picked chords as a progression in numerals, so it works in any key. Stored with the app's settings.
- **Detect chords** from a clip.
- MIDI Glass showing the current chord, and **Next chord** on the remote.

## 10. Order of work

1. The row, the model, saving, editing, the key picker, and the chord picker without suggestions. **Default output** and **Play chords**.
2. Suggestions, **More chords**, progressions, **Fill**, **Suggest**, Roman numerals and their setting.
3. Following: **Song key**, **Chord tones**, the arpeggiator's **Chords** source, **Follow key**, **Scale highlight**, **Scale snap**.
4. Chords to notes: drag onto a track, **Copy chords to new track**.
5. **Ask an AI assistant…**, its setting, and the article.
6. Sending: Flex Data to MIDI 2.0 devices, and key signatures in and out of MIDI files.
7. **Find key**, then the items in section 9.
