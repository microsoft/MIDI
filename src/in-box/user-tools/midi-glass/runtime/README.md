# The MIDI Glass runtime layer

Devices, connections and the things a running layout needs that are not drawing.

| File | Holds |
|---|---|
| `DeviceCatalog.*` | The layout's device table, resolved against what is plugged in right now. |
| `OutputRouter.*` | One connection per endpoint per process, shared by every control, page and layout. |
| `SurfaceScale.*` | Where a page sits inside a window, and which point on the page a point in the window is. |
| `PanicMessages.*` | Exactly what a panic sends. |

`SurfaceScale` and `PanicMessages` are free of `pch.h`, XAML and the MIDI SDK, so the unit tests compile them unchanged. `DeviceCatalog` and `OutputRouter` are not: one wraps a device watcher and the other owns a MIDI session.

## DeviceCatalog

A layout names its devices, and a device entry carries match criteria rather than a device id — that is what lets a layout built on one PC find the right hardware on another. `DeviceCatalog` turns "the layout wants a thing called Main Synth" into "that is endpoint `\\?\swd#...`, and it is here".

Matching itself is `midiapp::EndpointCatalog`, the shared code MIDI Patchbay uses, so both tools find the same hardware the same way. This layer only supplies the layout's own table and reports what came back.

- **A missing device keeps its place in the table.** The indexes the binding engine resolved at load time have to stay valid, so an absent device is an entry with no endpoint id rather than a gap.
- **It reports which groups the layout drives**, per device, because that is what Panic needs to be loud where the layout was playing and silent everywhere else.
- Changes are raised on the watcher's own thread. Nothing here calls up into the UI; the window marshals.

## OutputRouter

**One connection per endpoint, per process.** Group and channel travel inside the message, so hundreds of controls spread over four groups of one device still cost exactly one connection.

That is not only tidiness. It is what makes Panic mean *everything this app is driving* rather than *everything this window is driving*, and it is what stops two layouts pointing at the same synth from fighting over it. Two processes would give us two of everything and no way to reconcile them.

- **Everything here blocks on the service, so it is called from a background thread.** Blocking the UI thread hangs the app.
- The send table it hands back is used **from the UI thread and nothing else**, which is why the path a finger takes never waits on a lock. The table is swapped whole rather than edited.
- Connections nothing wants any more are closed **before** anything new is opened, so a machine with one single-client device can hand it from one layout to the next.
- The received callback is registered **before** the connection is opened, because it cannot be added afterwards without tearing the connection down.
- Feedback handlers are published as a settled list behind their own lock, so a service callback thread never waits on the lock a blocking open is holding.

## SurfaceScale

A page has a fixed pixel size and is **never stretched**, because a control surface is muscle memory and a control that changes shape stops being recognizable.

- **Actual size is the default**, and it scrolls when the page is bigger than the window. A page that did not fit starts hard against the origin rather than centered, because a page centered on its own scroll extent hides its top row.
- **Fit** scales both axes by the same number and letterboxes what is left. It can scale *up*: a performer on a large touch monitor wants the surface filling it.
- A point in the letterbox is **not** a point on the page, and is not treated as the nearest control.

## PanicMessages

Sustain off, all notes off, all sound off, pitch bend center — in that order, because asking for silence before the pedal is up does not stay silent.

Built as MIDI 1.0 protocol on purpose. A panic has to work on the oldest thing plugged in, and every one of those four is a message any device understands. The service still converts it for whatever is on the other end.

## What this does not do

- **It does not decide what to send.** That is the binding layer. The router is handed words and a destination index.
- **It does not queue.** A control whose device is missing stops sending. Stale MIDI arriving late is worse than nothing.
- **It does not own window placement, scroll position or zoom.** `SurfaceScale` answers a question; the window acts on the answer.
- **It does not create or destroy devices.** No loopbacks, no virtual devices, no configuration file. The virtual device a layout can publish arrives later and will be a session-scoped device, never a loopback.
- **It does not re-initialize a layout when a device comes back.** Startup values are sent once per run; the window owns that decision.
