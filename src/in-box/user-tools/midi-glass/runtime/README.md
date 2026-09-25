# The MIDI Glass runtime layer

Devices, connections and the things a running layout needs that are not drawing.

| File | Holds |
|---|---|
| `LivePlayer.*` | Everything between a value on the surface and words on the wire. The runtime window and the editor's Try mode both drive one. |
| `DeviceCatalog.*` | The layout's device table, resolved against what is plugged in right now. |
| `OutputRouter.*` | One connection per endpoint per process, shared by every control, page and layout. |
| `SequenceRunner.*` | The clock behind a sequence, a dump with gaps in it, and anything else that has to happen later rather than now. |
| `SurfaceScale.*` | Where a page sits inside a window, and which point on the page a point in the window is. |
| `PanicMessages.*` | Exactly what a panic sends. |

`SurfaceScale` and `PanicMessages` are free of `pch.h`, XAML and the MIDI SDK, so the unit tests compile them unchanged. `LivePlayer`, `DeviceCatalog` and `OutputRouter` are not: one opens connections, one wraps a device watcher and one owns a MIDI session.

## LivePlayer

The device table, the connections, the binding engine and the per-control throttles, in one place.

**It exists so that a control sends exactly the same thing while it is being built as it does once the layout is running.** The editor's Try mode and the runtime window each own one. Two copies of this code would drift apart, and the one place a customer would notice is the place they can least afford it.

- **Held by `shared_ptr`.** It starts detached threads that block on the service, and they must not call into a freed object when a window closes under them. `Create()` is the only way to make one.
- **A window and the same layout being edited are two owners**, so closing one does not take the other's connections down.
- **`Sent` is raised for every message that actually reached a connection**, which is what the editor's monitor rail draws. The runtime window leaves it empty and pays nothing for it.
- **`UpdateDocument` is for Try mode**, where an edit has to reach the engine before the next finger does. It forgets the destination signature deliberately, because an edit can change what a control sends without changing which devices the layout wants.
- It hands the page-item mapping back to the caller. The editor and the runtime window draw the same control differently, so what a feedback message does on screen is theirs to decide.

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

## SequenceRunner

One thread for the whole player, not one per sequence. Twenty buttons holding twenty sequences is a real layout, and twenty threads each waking every few milliseconds is not a reasonable way to spend a laptop's battery during a set.

Three rules, all of which came out of driving it rather than reading it:

- **Everything before the first wait runs on the caller's thread**, which is the pointer handler. A button carrying one dump has to feel like a button carrying one note. Only a wait puts a plan on the clock.
- **Sending always happens on the dispatcher's thread**, because the send table is read from the UI thread and nothing else. That is what lets the hot path take no lock, and the clock thread never touches a connection.
- **A run handed to the dispatcher is not due again until it comes back.** Without that flag, one slow frame turns a due run into a thousand work items.

The wait is a condition variable with a **predicate**, not a bare `wait_for`. A notification landing between "is anything due" and "wait" is otherwise dropped, and every step of every sequence stalls for the full idle second — which looks exactly like a sequence stopping half way through.

## What this does not do

- **It does not decide what to send.** That is the binding layer. The router is handed words and a destination index.
- **It does not queue.** A control whose device is missing stops sending. Stale MIDI arriving late is worse than nothing.
- **It does not own window placement, scroll position or zoom.** `SurfaceScale` answers a question; the window acts on the answer.
- **It does not create or destroy devices.** No loopbacks, no virtual devices, no configuration file. The virtual device a layout can publish arrives later and will be a session-scoped device, never a loopback.
- **It does not re-initialize a layout when a device comes back.** Startup values are sent once per run; the window owns that decision.
- **The runner does not decide what a sequence contains.** It is handed a flat list of actions; expanding a repeat block and building the words is the binding layer's job, at load time.
- **The runner never unwinds.** Stopping a sequence half way leaves what it already sent where it is, because guessing at somebody's synthesizer state is worse than leaving it alone. Panic is the thing that puts a rig back.
