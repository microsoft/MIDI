# MIDI Glass prototype

Exploratory work toward **MIDI Glass**, a customizable MIDI control surface for Windows in the Windows MIDI Services tool family. Think TouchOSC or TouchDAW, but the app itself looks like the rest of the family and the surface the customer builds does not.

**Nothing in this folder ships.** The design record and the spikes live here; **the app itself is real and lives in `src/in-box/user-tools/midi-glass/`.**

## Status

**Phases 0, 1, 2 and 3 are done. Phase 4 is part done.** The full state of play, the build and test commands, and the ordered list of what is left are at the **top of `design/MIDI-Glass-implementation-plan.md`** — start there, not here.

In short: the rendering approach is settled (a hybrid of a light XAML element per control with custom-drawn content, with a measured latency budget), the SDK sequencing work landed, the app shell exists, the document, theme and thumbnail layers are built, and the binding layer that decides what goes on the wire is built and tested. What remains in phase 4 is the device catalog, the output router, the runtime window, the renderer, input and multi-touch, Panic, and an end-to-end capture.

**98 unit tests, none of which need a window or a device**, in `src/in-box/Test/Tools/Midi2.MidiGlass.unittests`.

## Layout

| Path | What it is |
| --- | --- |
| `design/` | The design record: 14 HTML mockup screens, the design document, the implementation plan and the phase 0 findings. This is the source of truth for what the app is. |
| `design/shots/` | PNG captures of all 14 screens, so they can be reviewed without running anything. |
| `spikes/` | Console and WinUI spikes, and the raw measurements they produced. |

## Where the code actually is

| Path | What it is |
| --- | --- |
| `src/in-box/user-tools/midi-glass/` | The app. Each engine folder has its own `README.md` stating its contract, its invariants and **what it does not do**. |
| `midi-glass/document/` | The layout document, its file format, page templates, the theme model and the theme store. |
| `midi-glass/thumbnail/` | Drawing a library card with no window, via Win2D. |
| `midi-glass/binding/` | What a control puts on the wire, and how often. |
| `src/in-box/Test/Tools/Midi2.MidiGlass.unittests` | All of the above, tested without a window or a device. |

## The design record

Start with the three documents:

- **`design/MIDI-Glass-implementation-plan.md`** — the phases, the engine layering, the WinRT API work, and how each phase is verified. **This is the work order, and its first section is the current state.**
- **`design/MIDI-Glass-phase-0-findings.md`** — how a control gets painted and why, the latency budget, and what was not tested.
- **`design/MIDI-Glass-design.md`** — what the app is and why. Section 14 holds the settled decisions, section 12 holds the sequencing API analysis.


To look at the mockups, either open `design/shots/*.png`, or serve them:

```powershell
cd design
pwsh -File serve.ps1          # http://127.0.0.1:8742/
```

A plain `file://` open will not work for the linked stylesheet in some browsers, which is why the tiny static server is here. It is an unelevated `TcpListener`; `HttpListener` would need an admin URL ACL.

## Why the mockups are HTML

They are faster to iterate than XAML, they render the surface visual language accurately enough to judge color and contrast, and they can be captured reproducibly for review. They are a design tool, not a prototype of the implementation — none of this HTML or CSS becomes product code.

## What the phase 0 spike had to answer

How a control is painted, measured rather than argued:

1. A templated XAML element per control — automation, hit testing and theming are free, but the visual tree is heavy at 200 controls.
2. One custom-drawn canvas — fast and total control of the look, but every automation peer is hand-written and code-built visuals do not re-theme themselves.
3. A hybrid — a light XAML element per control for identity, hit testing and automation, with custom-drawn content.

**The hybrid won.** It costs what pure composition costs on every frame, it is the only one besides templated XAML that a screen reader can see at all, and it is the only one with a cheap answer for control labels. The numbers and the method are in `design/MIDI-Glass-phase-0-findings.md`; the raw results are in `spikes/results/`.

