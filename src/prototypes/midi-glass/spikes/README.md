# MIDI Glass spikes

Phase 0 of `design/MIDI-Glass-implementation-plan.md`. These exist to answer two questions with numbers instead of argument, and then to be thrown away. **Nothing here ships**, nothing here is in a solution, and nothing here is in the installer.

| Spike | Question | State |
| --- | --- | --- |
| `glassspike-surface` | How is a control painted? Three approaches, one measurement. And what is the floor on input-to-send latency? | Answered 23 September 2026 |

**The answers are in `design/MIDI-Glass-phase-0-findings.md`.** Raw measurements are in `results/`. In short: the hybrid wins, a screen reader finds nothing at all on a composition-only surface, and input to send is 264 µs of which our own code is 16 µs.

## glassspike-surface

One WinUI 3 app that builds the same 200-control page three different ways and measures each.

| Mode | What it is |
| --- | --- |
| `xaml` | A templated XAML `Control` per control. Automation, hit testing and theming come from the framework. |
| `composition` | One XAML host element with everything below it drawn as composition visuals. Hit testing is by hand and there is no automation at all. |
| `hybrid` | A light XAML element per control for identity, hit testing, focus and automation, with composition-drawn content inside it. |

Every mode draws the same page, from the same `SurfaceModel`, with the same seed, so the only variable is how it is painted.

### What gets measured

- **Build time** — creating and realizing the whole page, including a forced layout pass.
- **Frame interval** — from `CompositionTarget.Rendering`, reported as p50, p95, p99, max and the count over 20 ms.
- **Animation cost** — microseconds of UI thread time spent updating the animating controls each frame, *including the layout pass that follows*. Leaving the layout out would make the templated XAML approach look free.
- **Value update cost** — microseconds to push one new value into one control. This is the repaint half of the hot path.
- **Send cost** — microseconds for `SendSingleMessageWords`, measured on its own so it does not need a hand on the screen.
- **Memory** — private bytes and working set, at baseline and after the page is built.
- **Element count** — how many XAML elements the page cost.
- **Theme swap** — how long six brushes take to recolor a whole page, and a read-back of every control to prove it landed.
- **Input to send** — the pointer timestamp the system stamped on the input, against the moment `SendSingleMessageWords` returned.

### Whether a run is worth believing

A performance run taken while somebody is using the machine is not evidence. Each run samples system and process processor time across its own duration and watches for the window losing focus, and reports itself **clean** or not. A result that is not clean should be discarded rather than explained.

The detector was validated before it was trusted: a quiet run reports about 11 % machine busy and clean, and the identical run under twenty one spinning background jobs reports 95 % busy and not clean.


### Running it

```powershell
cd glassspike-surface
pwsh -File build.ps1                 # Release x64
pwsh -File ..\run-spike.ps1          # all three modes, writes results next to the script
```

`build.ps1` passes `SolutionDir` for the referenced Windows MIDI Services SDK project, the same way every tool in this repository is built. Build the SDK once first if this is a clean tree.

By hand:

```
glassspike-surface.exe                                  interactive, pick a mode and press Run
glassspike-surface.exe --mode hybrid --controls 200 --animate 12 --seconds 20 --autorun --out results.json
```

| Switch | Default | Meaning |
| --- | --- | --- |
| `--mode` | `xaml` | `xaml`, `composition` or `hybrid` |
| `--controls` | 200 | how many controls on the page |
| `--animate` | 12 | how many of them animate |
| `--seconds` | 20 | how long a run lasts |
| `--endpoint` | loopback A | endpoint device id for the latency probe |
| `--autorun` | off | run once and exit, no interaction |
| `--out` | none | append the result to this file as one JSON object per line |

### Measuring input to send needs a hand

The latency number is the one thing here that cannot be produced without real input, because synthesizing pointer input takes the machine away from whoever is using it — and the system's own delivery time is the thing being measured, so a synthesized event would measure the wrong thing anyway. Drag a fader with a mouse, then with a finger, then with a pen, and the readout fills in per device type. A run that was never touched reports the input path as not measured rather than guessing.

The send cost on its own is measured every run and needs nobody.

### What this deliberately does not do

- It is not a prototype of the app. None of this code is meant to become `midiglass`.
- It has no `.resw`, no settings, no icon and no window chrome. Every string is inline, which is the opposite of what a shipping tool in this family must do. Do not copy the shape of this project.
- It does not try to be pretty. It draws enough of the plate, rim and light pipe to cost what the real thing would cost.
