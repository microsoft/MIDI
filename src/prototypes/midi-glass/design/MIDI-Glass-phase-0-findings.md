# MIDI Glass phase 0 — what the spikes measured, and what was decided

Phase 0 of `MIDI-Glass-implementation-plan.md` existed to settle two things a document cannot: how a control gets painted, and what the floor is on input-to-send latency. Both are now measured. This is the record.

Measured 23 September 2026 with `spikes/glassspike-surface`. Raw results are in `spikes/results/`.

---

## The decision

**Build the surface as a hybrid: a light XAML element per control for identity, hit testing, focus and automation, with the content drawn as composition visuals inside it.**

It was the hypothesis in the plan, and it won on the numbers rather than on the argument. It matches pure composition on every cost that repeats per frame, matches templated XAML on everything a screen reader needs, and it is the only one of the three that has an answer for control labels.

---

## How it was measured

One application builds the same page three ways from the same model with the same seed, so the only variable is how it is painted. Five page sizes, three approaches, two passes, ten seconds each — thirty runs.

A run can be ruined by somebody using the machine, so the spike judges its own results: it samples system and process processor time across the run and watches for the window losing focus, and marks each result clean or not. The detector was checked before it was trusted — a quiet run reports 11 % machine busy and clean, the identical run under twenty one spinning background jobs reports 95 % busy and not clean. **All thirty runs below were clean, and the two passes agree.**

Hardware was a Core Ultra 9 285K, x64, Release, at 60 Hz. That matters for reading the numbers: see the gaps at the end.

---

## Painting a control

Frame time, per-frame cost on the UI thread, and what the page cost to build.

| Page | | Templated XAML | Composition | Hybrid |
| --- | --- | --- | --- | --- |
| 200 controls, 12 moving | frames | 60 fps, 0 late | 60 fps, 0 late | 60 fps, 0 late |
| | UI thread per frame | 103 µs | 18 µs | **19 µs** |
| 200 controls, all moving | frames | 60 fps, 0 late | 60 fps, 0 late | 60 fps, 0 late |
| | UI thread per frame | 627 µs | 81 µs | **86 µs** |
| 600 controls, 60 moving | frames | 60 fps, 0 late | 60 fps, 0 late | 60 fps, 0 late |
| | UI thread per frame | 260 µs | 46 µs | **47 µs** |
| 1200 controls, 120 moving | frames | 60 fps, 1 late | 60 fps, 0 late | 60 fps, 0 late |
| | UI thread per frame | 485 µs | 86 µs | **85 µs** |
| **1200 controls, all moving** | frames | **9 fps, every frame late** | 60 fps, 0 late | 60 fps, 1 late |
| | UI thread per frame | **3005 µs** | 434 µs | **500 µs** |

"UI thread per frame" is the cost of pushing new values into the animating controls *and* the layout pass that follows. The layout pass is inside the measurement deliberately — for a tree of XAML elements that is most of the cost, and leaving it out would make the first column look free.

Build cost and footprint, for a 200 control page:

| | Templated XAML | Composition | Hybrid |
| --- | --- | --- | --- |
| Build the page | 11.0 ms | 1.6 ms | 4.8 ms |
| XAML elements created | 1251 | 1 | 201 |
| Private bytes | 1.9 – 3.7 MB | 1.1 MB | 1.6 MB |
| One value into one control | 0.35 µs | 0.09 µs | 0.10 µs |

At 1200 controls the build costs are 62 ms, 11 ms and 30 ms, and the footprints are roughly 12 MB, 8 MB and 9.4 MB.

### What is worth taking from this

**The XAML element in the hybrid is free per frame.** 19 µs against composition's 18 µs at the plan's own test size, and they stay level all the way to 1200 controls. The element is paid for once when the page is built and never again, because changing a value touches only the composition visual and invalidates no layout. That is the whole case for the hybrid and it measured better than expected.

**Templated XAML is fine at the size the plan specified, and the spike nearly said so.** 200 controls with 12 animating holds 60 fps with room to spare. It only falls over when a large page is *entirely* in motion. That is not a contrived case — it is a 120 channel desk sending feedback for everything at once — but a spike that had only run the specified test would have cleared an approach that collapses to 9 fps under a real load. **Sweep the size; do not test the one number in the plan.**

**Nothing was gained by writing the hit test by hand.** The composition renderer has to find its own control under the pointer because there are no elements to route to. The spike does a linear scan, which is fine at 1200 and would not be at 5000. The hybrid gets routing from XAML for nothing.

---

## What a screen reader can find

Driven through UI Automation against the live window, not reasoned about. The enumerator was validated first — 21 elements in the window with no page built, 221 with a 200 control page — because a broken query returns zero for everything and looks exactly like a broken application.

| | UIA sliders found | with a name | value readable | value settable by assistive tech |
| --- | --- | --- | --- | --- |
| Templated XAML | 200 | 200 | yes | yes |
| Composition | **0** | 0 | — | — |
| Hybrid | 200 | 200 | yes | yes |

A screen reader finds nothing at all on a composition-only surface. Not a slider with a missing name — nothing. Making it work would mean hand-writing a provider, a fragment root and a navigation model for a tree that has no elements in it.

The hybrid needed one automation peer class of about seventy lines, shared by both element types, exposing control type, name and the range value pattern. `RangeValuePattern.SetValue(0.25)` was called through automation and the control moved, so this is a working path and not a label.

---

## Input to send

15,100 real pointer moves from a mouse drag on the surface. Synthesized input was not used — it takes the machine away from whoever is using it, and the delivery time is the thing being measured, so a synthesized event would have measured the wrong thing.

| | p50 | p99 |
| --- | --- | --- |
| Pointer timestamp from the system to the message sent | **264 µs** | 1243 µs |
| Our own code inside the handler | 16 µs | 32 µs |
| `SendSingleMessageWords` on its own | **0.34 µs** | 0.80 µs |

The system's own delivery accounts for about 248 µs of the 264. Everything the application controls is 16 µs, or six percent.

The pointer timestamp was checked against `QueryPerformanceCounter` rather than assumed to share a timebase; the spike records whether that check passed and would have reported the number as untrustworthy if it had not.

### The latency budget

- **Build and send the message in the pointer event handler.** Queuing it to a render tick would turn a 0.26 ms path into as much as 17 ms — sixty times worse, for nothing. This is now measured rather than asserted.
- **Budget 100 µs for everything the handler does.** It costs 16 µs today with room for the binding lookup, the resolution fold and the throttle check.
- **`GetCurrentPoint` is about 15 µs — forty five times the cost of the MIDI send.** It is the expensive call on the hot path, not the MIDI one. Call it once per event and pass the point down. That is the opposite of where anyone would look first.
- The repaint that follows is a separate, lower priority concern, as the plan already says. At 0.10 µs per value it is not close to being a problem.

---

## Two other things the spike settled

**The theming trap did not bite any of the three.** All three share brush objects across controls rather than creating one per control, so recoloring six brushes re-themed all 1200 controls in 0.2 ms or less. A read-back of the color every control is actually painting with confirmed all 1200 changed, in every approach. The trap Patchbay hit is real, but it is a coding mistake — creating a brush per control — rather than a property of any of these approaches.

**`SendSingleMessageWords` is safe to call from the STA UI thread.** Opening the session and the connection is not, and is done on a background thread here, matching the rest of the tool family. The send is 0.34 µs and does not block.

---

## What was not measured

Stated plainly, because a phase exit is worth less without it.

- **No low-end hardware.** Everything here is one desktop with a Core Ultra 9 285K. Control count is standing in for a slower machine, which is not the same thing. The 1200-control column is the closest thing to a weak-hardware answer and it is not a substitute for running it on one.
- **No ARM64 run, and no Debug configuration.** Release x64 only.
- **No touch and no pen.** This machine has neither. The input number is mouse only, and multi-touch — two fingers on two faders, which the design says is the whole point — is untested.
- **No labels, no images, no deck image.** The spike draws the plate, the rim, the light pipe and the bloom. Every real control also has text. This is the biggest remaining unknown and it is worth being explicit about:
  - For the **hybrid**, a label is a XAML `TextBlock` inside the element. That is one more element per control, so about 400 for a 200 control page — still a third of what the fully templated approach costs.
  - For **composition only**, text needs a drawn surface, and Win2D is not in this repository's package set. There is no cheap answer.
  - This is a third argument for the hybrid, and it was not on the list before the spike.
- **No frame time under memory pressure or with a GPU under load from something else.**

---

## What phase 4 should carry forward

The spike is a measurement harness, not a prototype of the app, and none of its code should become product code. Three things in it are worth repeating rather than reinventing:

1. **A renderer interface that hides which approach is in use.** It cost nothing and it is what made a fair comparison possible. `SurfaceRenderer` in the real app should keep that shape, if only so a future measurement can be repeated.
2. **A page model generated from a fixed seed.** Both the comparison and every repeat depended on all three approaches being handed byte-for-byte identical work.
3. **A run that judges its own validity.** Any performance measurement taken on a machine somebody is using should say whether it is worth believing. It cost about forty lines and it caught a contaminated run within minutes of existing.
