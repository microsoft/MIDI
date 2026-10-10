# Windows MIDI Sequencer prototype

Design work for **Windows MIDI Sequencer**, a MIDI 2.0 sequencer in the Windows MIDI Services tool family. It records and plays MIDI only. It has no audio engine. It combines clip launching, step patterns, generators and an ordinary timeline in one window, and it's planned as a WinUI 3 app in C++, like the rest of the family.

**Nothing in this folder ships.** This is the design record: comps and a written design. The app's code is in `src/in-box/user-tools/midi-sequencer/` (the sequence model, files, undo and the playback engine so far; no window yet), and its tests are in `src/in-box/Test/Tools/Midi2.MidiSequencer.unittests/`. Section 20 of the design says how far each phase has got.

## Layout

| Path | What it is |
| --- | --- |
| `design/MIDI-Sequencer-design.md` | The written design: what the app is, how it's organized, how step patterns and generators fit in, what it reuses, file formats, phases and the decisions still open. Start here. |
| `design/index.html` | The list of comps. Each comp is an HTML page. Open it in a browser. |
| `design/1-main.html` to `design/8-provenance.html` | The comps. `seq.css` and `seq.js` draw them; the notes, steps, rings and automation are drawn from data in `seq.js`, not hand-placed. |
| `design/shots/` | PNG captures of every comp, so they can be reviewed without opening a browser. |
| `design/capture.ps1` | Captures every comp to `shots/` with headless Edge. Nothing appears on screen. |
| `design/check.ps1` | Reports layout problems in every comp (text cut off, clips on top of each other, things outside their window) without taking a picture. |

## Updating the comps

```powershell
pwsh -File design\check.ps1          # every page, or -Page 3 for one
pwsh -File design\capture.ps1        # writes design\shots\*.png
```

Both need Microsoft Edge in its usual place. Each page sizes its own capture: `capture.ps1` opens it once with `?check` to learn how big it is.
