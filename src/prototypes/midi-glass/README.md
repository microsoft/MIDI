# MIDI Glass prototype

Exploratory work toward **MIDI Glass**, a customizable MIDI control surface for Windows in the Windows MIDI Services tool family. Think TouchOSC or TouchDAW, but the app itself looks like the rest of the family and the surface the customer builds does not.

**Nothing here ships. Nothing here is wired into the service or the installer.** When the app is real it will live in `src/in-box/user-tools/midi-glass/` like every other tool.

## Status

**Design is complete and approved. Phase 0 is done.** The rendering approach is settled — a hybrid of a light XAML element per control with composition-drawn content — and the hot path has a measured latency budget. See `design/MIDI-Glass-phase-0-findings.md`.

The next piece of work is phase 1, the sequencing API changes, and phase 2, the application shell. They touch nothing in common and can run at the same time.

## Layout

| Path | What it is |
| --- | --- |
| `design/` | The design record: 14 HTML mockup screens, the design document, the implementation plan and the phase 0 findings. This is the source of truth for what the app is. |
| `design/shots/` | PNG captures of all 14 screens, so they can be reviewed without running anything. |
| `spikes/` | Console and WinUI spikes, and the raw measurements they produced. |

## The design record

Start with the three documents:

- **`design/MIDI-Glass-implementation-plan.md`** — the phases, the engine layering, the WinRT API work, and how each phase is verified. This is the work order.
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

