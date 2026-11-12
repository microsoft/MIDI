# Windows MIDI Calculator: design proposal

Status: comps only, November 11 2026. No code yet. Open `index.html` in a browser for the comps, or run `capture.ps1` for pictures in `shots`.

## What it is

A small calculator for musicians and developers, built like the other Windows MIDI tools. You keep it open next to your music app. It answers the questions that come up while you work: what tempo a loop is, how long a dotted eighth is, what note 450 Hz is, how far apart two MIDI timestamps are, and what number to put in a MIDI 2.0 message for a pitch.

It takes ideas from the Cubase beat calculator people keep asking Steinberg to bring back, from musiccalculator.com, from the musictheory.net chord calculator and from the MusicMath app. Two parts are ours alone: Windows timestamps, and MIDI 2.0 values such as Pitch 7.9.

## Pages

| Page | What's on it | Comp |
| --- | --- | --- |
| Tempo and beats | Tap tempo (mouse, keyboard or a MIDI note), beat calculator (tempo, length or bars from the other two), note lengths for delays (ms, Hz, samples, ticks), tempo change, measure incoming MIDI Clock | `1-tempo-and-beats.html` |
| Notes and pitch | Note to frequency, frequency to note with cents, intervals, note table | `2-notes-and-pitch.html` |
| Chords and scales | Chord name to notes, notes to chord name (click or play), chords in a key with Roman numerals | `3-chords-and-scales.html` |
| Timecode and samples | Timecode calculator with a tape, convert to frames, real time, samples and bars, move between frame rates, buffer lengths | `4-timecode-and-samples.html` |
| MIDI 2.0 values | Pitch 7.9 and 7.25, pitch bend in both protocols, value scaling between resolutions, JR Timestamps and Delta Clockstamps, decode UMP words, manufacturer IDs | `5-midi-2-values.html` |
| Windows timestamps | Live MIDI clock, timestamp to time and clock time, the gap between two timestamps, FILETIME, WinRT and .NET ticks, Unix time | `6-windows-timestamps.html` |
| Compact overlay | One calculator in a small always-on-top window | `7-compact-overlay.html` |
| History | Answers you copied, used or pinned | `8-history.html` |

A strip of **session values** (tempo, meter, sample rate, tuning reference, frame rate) sits above every page. You set them once and every calculator uses them. Tap tempo, the beat calculator and Measure MIDI Clock can each write their answer into it.

## How it's built

The same way as the other tools, so it looks and behaves like them and costs little to keep up.

- A WinUI 3 C++/WinRT desktop app in `src\in-box\user-tools\midi-calculator`, started from `midiscratchpad` the way every tool is.
- From `midi-app-shared`: `WindowChrome` (title bar, backdrop, window placement), `MidiAppSettings` (saved settings under the tools' registry key), the appearance flyout, the PREVIEW badge, `SingleInstance`, the endpoint pickers, `NoteNames.h`, `MidiTimeCode`, `UmpDescriber` and `ClockFollower`.
- Two pieces move into `midi-app-shared` so they aren't copied: `TapTempo` from the MIDI Clock app, and the chord and scale naming in the Sequencer's `Harmony`. `Harmony` uses the Sequencer's `ChordEvent`, `KeyChange` and `ScaleKind` types, so the plan is to move the plain chord and scale part into a shared `MusicTheory.{h,cpp}` and leave the Sequencer's row and file code where it is.
- All the math lives in plain C++ files with no XAML and no WinRT: tempo, pitch, timecode, MIDI 2.0 and Windows time. Each gets unit tests, and every number in these comps becomes a test case. The numbers were worked out with a script, not by hand.
- The timecode page needs frame rates MIDI Time Code doesn't carry (23.976, 29.97 non-drop, 50, 59.94, 60), so it gets its own timecode math. It's tested against the shared `MidiTimeCode` for the four rates they share.
- MIDI is optional. The calculator works with the MIDI service stopped. It opens an endpoint only when you ask it to: tapping from a MIDI note, measuring MIDI Clock, naming a chord you play, or playing a note or chord. Service status uses `IsMidiServiceRunning()`, never `EnsureServiceAvailable()`, which starts the service.
- Windows timestamps come from the SDK's `MidiClock` (`Now`, `TimestampFrequency`, `GetCurrentSystemTimerInfo`). No new SDK API is needed.
- It's a new app that ships with the Tools, not code that ships in Windows, so it needs no servicing gate.
- Every string is in `Resources.resw`, in en-US, with sentence-case headings. Each result has an accessible name that includes its label and value, and the tap tempo result is a live region so a screen reader announces it.

## Command line and other apps

`midicalc <page> [values]` opens a page with values filled in, the same way the other tools take a command line. Examples: `midicalc tempo --bpm 128`, `midicalc timestamp 2892714813833`, `midicalc ump 40904503 C9248AC7`.

That lets other tools hand off to it. The first one I'd add is in the MIDI Monitor: select two messages and choose "Time between these" to open Windows timestamps with both filled in.

## The math, and where each rule comes from

- **Note names.** Middle C (note 60) is C3, as in every Windows MIDI tool (`NoteNames.h`). Settings offers C4.
- **Pitch 7.9** (Note On attribute type 3): 7 bits of semitone (the spec calls it an HCU, a 100-cent unit), then 9 bits of fraction. M2-104-UM v1.1.2, section 7.4.
- **Pitch 7.25** (Registered Per-Note Controller #3, opcode 0x0): 7 bits of semitone, 25 bits of fraction. M2-104-UM 7.4.15.2.
- **Pitch bend.** MIDI 2.0 pitch bend is unsigned and centered at 0x80000000. In MIDI 2.0, RPN 0 (bend range) carries semitones in the top 7 bits of the 32-bit value and cents in the next 7. M2-104-UM 7.4.7.1.
- **Value scaling.** Min-center-max is the default for velocity, controllers, pressure and pitch bend. Zero-extension is for RPNs with an index of 0 to 31 and for pitch values, so Pitch 7.9 widens to 7.25 by adding zeros. M2-115-U v1.0.2, sections 3 and 4.
- **JR Timestamps** count in 1/31,250 s (32 µs) ticks. M2-104-UM.
- **Delta Clockstamps** count in ticks per quarter note set by the DCTPQ message. M2-104-UM, MIDI Clip File section.
- **Manufacturer IDs** convert between the 1- and 3-byte MIDI 1.0 forms and the 16-bit MIDI 2.0 form with the spec's example code. M2-104-UM.
- **MIDI timestamps** are `MidiClock` ticks. Today that's the same counter as `QueryPerformanceCounter`, but the docs tell developers to use `MidiClock`, so the calculator does too and says "MIDI timestamp", not "QPC".
- **Windows time values.** FILETIME and WinRT `DateTime` count 100 ns from January 1 1601 UTC. .NET `DateTime` counts 100 ns from January 1 year 1. Unix time counts from January 1 1970 UTC. `MidiSystemTimerSettings` reports the system timer in 100 ns units.
- **Drop frame.** 29.97 drop frame skips two frame numbers at the start of each minute, except every tenth minute. 59.94 drop frame skips four. The shared `MidiTimeCode` already does this for 29.97.

## Not in the first version

- Temperaments other than equal temperament (see the open questions).
- Playing sine tones. The calculator plays notes and chords through MIDI instead, to the Windows MIDI Sound Module or an endpoint you pick.
- Finding the tempo of an audio file.
- Changing anything in another app. It reads and copies values; it doesn't control your music app.

## Phases

1. The window, session values, History, Tempo and beats, MIDI 2.0 values and Windows timestamps. The last two are the pages nobody else has.
2. Notes and pitch, Timecode and samples, the command line and the Monitor hand-off.
3. Chords and scales (after the shared chord code move), the compact overlay, and the MIDI input features (tap from a note, measure clock, name a chord you play).

## Decisions (Pete, November 11 2026)

1. **Name.** Windows MIDI Calculator. It's broader than MIDI, but the name stays. The app is `midicalculator.exe` in `src\in-box\user-tools\midi-calculator`.
2. **Middle C.** Aligned with the Windows MIDI Services API: C3 by default, with the octave in Settings.
3. **Temperaments.** A short list from the start: equal temperament, just intonation, Pythagorean, quarter-comma meantone and Werckmeister III, each built on any root.
4. **Shared code.** Yes. `TapTempo` is now in `midi-app-shared`; the Sequencer's chord code moves with the Chords and scales page.
5. **Console.** Not now. The calculator is a GUI app only.
6. **History.** The last 500 answers, kept between runs, with pinning.

## Built so far (November 11 2026)

Tempo and beats, Notes and pitch, MIDI 2.0 values, Windows timestamps and History, laid out to these comps. Still to come: Chords and scales, Timecode and samples (with the session frame rate), the compact overlay, and the MIDI input features (tap from a note, measure MIDI Clock, play a note or chord).