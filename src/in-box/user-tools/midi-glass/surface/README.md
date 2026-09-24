# The MIDI Glass surface layer

Painting a page, and turning a finger on it into a value.

| File | Holds |
|---|---|
| `GlassControl.*` | One XAML element per control: a place in the tree, hit testing, focus, automation. |
| `SurfaceAutomationPeer.*` | What that element owes a screen reader. |
| `SurfaceRenderer.*` | The composition visuals drawn inside each element. |
| `InputRouter.*` | Pointer, pen and touch to a control to a value. |
| `InputRules.*` | Which way a finger moves a control. Pure, and tested. |
| `SurfaceColors.*` | What color a control ends up, given a theme. Pure, and tested. |

`InputRules` and `SurfaceColors` are free of `pch.h` and XAML so the unit tests compile them unchanged. The rest need a window.

## The hybrid, and why

Phase 0 measured three ways of painting a control and this is the one that won, on numbers rather than on argument. The record is in `src/prototypes/midi-glass/design/MIDI-Glass-phase-0-findings.md`.

- **The XAML element costs nothing per frame.** 19 µs against pure composition's 18 µs at 200 controls, level all the way to 1200. It is paid for once when the page is built; changing a value touches only the composition visual and invalidates no layout.
- **A screen reader finds nothing at all on a composition-only surface.** Not a slider with a missing name — zero elements. The hybrid exposes every control as a named slider whose value can be read and set.
- Pointer routing and **per-pointer capture** come free with the element, and that is what gives multi-touch: two fingers on two faders, which is the entire point of the product.

## The hot path

From a finger touching glass to a message leaving there is one frame of budget and no allocations.

- The message is built and sent **in the pointer handler**. Measured: 264 µs that way, as much as 17 ms if it were queued to a render tick.
- **`GetCurrentPoint` is the expensive call, not the MIDI send.** About 15 µs against 0.34 µs. It is called once per event and the point is passed down. That is the opposite of where anyone would look first.
- Budget 100 µs for everything the handler does.

## How a control responds to a finger

| Kind | What a touch does |
|---|---|
| Fader, XY pad | Jumps to where it was touched, and follows |
| Knob, encoder | Nudged, not set. A knob has no travel under the finger, so jumping would make every touch a wild move |
| Pad, button, page tab | Momentary: sends on press and again on release |
| Toggle | Flips on press |
| Meter, lamp, readout, label, image | Nothing. They display |

**One finger per control.** A second pointer on a control somebody is already using is ignored rather than fought over, which would make the value jump between two positions.

**A fader laid out wider than it is tall is a horizontal fader.** The axis follows the rectangle rather than the name of the control.

**Screen coordinates run downward and a fader does not.** That is the one piece of this arithmetic that is easy to get backwards, and a fader that reads upside down is the kind of defect nobody finds until it is on stage. It has a test.

**While View mode is on the surface sends nothing.** A pinch on a control surface is ambiguous — two fingers might be two fingers on two faders — so zoom and pan live behind an explicit switch rather than being guessed at.

## Colors

A control stores a **hue slot**, not a color, so switching theme is a six-color operation rather than a redesign. One control never uses more than one hue, and a hue never appears anywhere except the rim, the value pipe and the bloom.

The plate is worked out from the theme rather than stored per control:

| The theme says | The plate is |
|---|---|
| A plate color outright | That color. Bigwig is the one shipped theme that does this, because its whole idea is that orange only ever means "this is the value" |
| A fill at rest | The deck tinted with the control's own hue. This is why the tonal themes cost no extra rendering layer — a tint is a background color |
| Neither | Near black at the theme's glass tint, over whatever the deck is. A tint of zero means the deck shows through untouched, which High contrast asks for |

**One brush per distinct color for the whole page.** A control never owns a brush. That is what keeps a theme swap a handful of objects rather than a walk of two hundred, and it is the trap MIDI Patchbay hit — creating a brush per control is a coding mistake, not a property of the approach.

**A label's ink is chosen by measuring the background**, not by assuming the theme is dark. A theme with a mid grey deck gets whichever of the two candidates wins.

**The lamp ring falls back to a solid arc below the theme's own size floor.** Measured: at 36 px the lamps stop separating and the ring reads as a fine comb.

## Reduced motion

The surface is a wall of animation by design, which is exactly why honoring the Windows setting is not optional. With reduced motion on, the bloom **switches** instead of fading. It still says "this one just did something", which is the whole job.

Bloom decay is a composition animation, so a page of blinking controls costs the UI thread nothing.

## What this does not do

- **It does not send anything.** It raises a value; the window owns the connection and the send.
- **It does not know what a control is bound to.** Group, channel, controller number and the range all live in the binding layer.
- **It does not scale the page.** It draws in page coordinates; the window applies the transform.
- **It does not lay anything out.** A control is where the document says it is. Snapping, guides and the grid belong to the editor, which does not exist yet.
- **It does not edit.** No selection, no drag to move, no handles.
- **It does not draw pages other than the one on screen.** A page change rebuilds.
- **It does not handle an XY pad's second axis, or draw a meter from anything but its own value.** Both are honest gaps, not design decisions.
