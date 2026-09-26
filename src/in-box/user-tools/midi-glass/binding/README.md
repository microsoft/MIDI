# The MIDI Glass binding layer

What a control puts on the wire, and how often.

| File | Holds |
|---|---|
| `BindingEngine.*` | A control moving to the words that leave the process. Resolution folding, routing by index, startup values, feedback in. |
| `ValueThrottle.*` | The rate limit on a continuous control, and its trailing send. |
| `ActionPlan.*` | Everything a control does that is **not** an immediate channel voice message: a system exclusive dump, a raw message, a sequence of steps, a jump to another page. Flattened at prepare time into a straight list of actions. |
| `LearnCapture.*` | An incoming message read back into a binding, and the rules for which of them are worth acting on. |
| `MonitorFormat.*` | Words decoded back into something a person can read, for the editor's monitor rail. |

## The hot path

From a finger touching glass to a message leaving there is **one frame of budget and no allocations**. Everything expensive happens in `Prepare`; `Evaluate` only does arithmetic into a caller-owned buffer.

- **Device names are resolved to indexes when the layout loads.** After that there is no string comparison and no map lookup on the path a fader takes.
- `PreparedSend` is a fixed-size struct, so the caller owns an array of them on the stack.
- Messages are built and sent **in the pointer handler**, not queued to a render tick. A render tick is up to 16 ms of latency for nothing. The repaint that follows is a separate, lower-priority concern: if a frame is dropped, the MIDI already went.

## Protocol, and why this layer does not fold anything

**The app always sends UMP, and by default it sends MIDI 2.0 protocol at full resolution.** The service does the downscaling — both MIDI 2.0 protocol to MIDI 1.0 protocol, and UMP to MIDI 1.0 byte format — for whichever of the *client* or the *device* needs it.

That is not just less code. The service owns the canonical conversion, every other app on the machine gets the same one, and it can do things this layer cannot: one registered controller sent as `40200509 12345678` arrives at a WinMM client as the four MIDI 1.0 messages that carry it — CC 101, CC 100, CC 6, CC 38. An app that folded to seven bits itself would have to reimplement that, and the two would drift.

So a destination's protocol makes **no difference to what leaves here**. There is a test that asserts exactly that.

### Ranges, and values that are codes rather than positions

Every message has a **minimum** and a **maximum**, and each end is independently a **percentage** or an **absolute number**. A control at rest sends the minimum, a control at full travel sends the maximum, and anything between is interpolated and rounded.

That is one idea rather than several, and it is why there is no separate on/off pair:

| What the customer wants | How it is expressed |
|---|---|
| An ordinary fader | minimum 0 %, maximum 100 % — the default |
| A fader limited to MIDI 1.0 range | minimum 0, maximum 127, absolute. The rounding quantizes it onto whole numbers |
| A button, off and on | minimum 0, maximum 127 |
| A button on a MIDI 2.0 device | minimum 17, maximum 13005 |
| An APC40 clip LED | maximum 5 absolute, for `#FF0000` |
| A fader that reads top to bottom | minimum above maximum — inversion falls out of the arithmetic |

Device documentation does not talk in percentages. The APC40 Mk2 protocol says a clip LED is a note on where the note number picks the LED, the **channel** picks the display type and the **velocity** picks the color — channel 0 for a solid primary color, channel 9 to pulse at a quarter note, velocity 5 for `#FF0000` and 21 for `#00FF00`. A customer copying that table has to be able to type **5**, and to see 5 in the editor afterwards, not 3.9 %.

**`UseMidi1Protocol`** is a separate switch again: build the message as MIDI 1.0 protocol, still UMP format, when the device's documentation is written in MIDI 1.0 terms. It is independent of the units, because the same need for exact values arrives with 16-bit MIDI 2.0 velocities.

`RawUmp` is still there for someone who would rather type the words themselves.

Verified on the wire: `20900005` decodes as note 0 velocity 5 on channel 1, `20990015` as note 0 velocity 21 on channel 10, and `40903C00 75300000` as a MIDI 2.0 note on at velocity 30000.

### Detents — a continuous control with stops

A control does not have to be smooth. `Detents` on a message gives it stops, in one of three shapes:

| Mode | What it means | Example |
|---|---|---|
| `Continuous` | No stops. The default. | A volume fader |
| `EvenSteps` | A step in the same units as the ends | 0 to 100 % in steps of 10 %, or 27 to 127 in steps of 5 |
| `ExplicitValues` | An arbitrary list | Stops at 10, 17, 38, 39, 40 and 57 |

**A listed stop gets an equal share of the travel**, rather than sitting where its value falls between the ends. That is not a detail — it is the only way the third example works. Spaced by value, 38, 39 and 40 are a fortieth of the travel apart, and a test of that version reaches `10 17 57 57 57 57`: three of the six stops cannot be selected at all.

`EvenSteps` is measured from the **minimum**, so a range that does not start at zero still has a stop exactly on its own bottom end. A zero step or an empty list falls back to smooth rather than dividing by zero or sending nothing.

`DetentCount` and `DetentPosition` are there for whatever moves the control: the engine produces the right value either way, but the surface needs to know where to snap a finger and where to draw the notches.

## Resolution folding

A value is stored once as a fraction of full scale, so the same layout file is correct on a MIDI 1.0 device and a MIDI 2.0 one and nobody has to know which they have.

- 1.0 scales to the **top** of the range, not one short of it. A fader pushed all the way up sends the maximum the wire can carry.
- A program number is an **identity, not a position**, so it is never scaled. Scaling it would recall a different patch depending on where a fader happened to be.
- A note is on or off, not a position; the velocity comes from the row's own on and off values.

Verified against an independent decoder rather than against the arithmetic that produced it: the words go through a loopback and the MIDI console decodes them.

## The throttle

A DIN cable carries about 350 three-byte messages a second, shared with everything else on that wire, so a continuous control needs a rate limit. It sits on the value-changed notification rather than inside the engine, so one limit governs both the send and the repaint and there is no way for the surface to show a value that was never sent.

- **The first move of a gesture always goes**, however heavy the limit.
- **The last value is always sent.** This is the rule that gets forgotten and the one that matters: without it a fader settles a few units from where the finger left it, and the surface and the desk disagree for the rest of the session.
- **Nothing else is throttled.** Rate limiting a note on would be a defect, not a feature.
- Time is passed in rather than read, so the trailing send is testable without waiting.

## What this does not do

- **It does not open a connection or send anything.** It produces words and a destination index. Owning connections is the router's job, one layer up.
- **It does not queue.** A control whose device is missing stops sending. Stale MIDI arriving late is worse than nothing, and a layout that buffered a minute of fader moves would dump them all at once when the device came back.
- **It does not know about pointers, frames or windows.** It is handed a value.
- **It does not decide when to send.** The throttle answers "may this go now"; something above it does the sending.
- **It has no clock.** `ActionPlan` says what happens and how long to wait between; something above it owns the waiting.
- **It does not listen.** `LearnCapture` decodes a message it is handed; opening a connection to hear one is the runtime layer's job.

## Plans — the things that are not on the hot path

A control change leaves in the pointer handler. A dump, a raw message, a sequence and a page change do not: they can be thousands of packets, or they can have real gaps in them. `ActionPlanSet::Prepare` turns all of those into one flat list per control and trigger.

**A plan is flattened, so the thing that runs it has no control flow at all.** A repeat block is expanded at prepare time, bounded at 4096 actions and four levels of nesting. That is what makes a plan readable in the editor, cheap to run, and impossible to turn into a loop that never ends — which matters, because a layout is untrusted input from a stranger.

Two rules that came out of driving it on the wire rather than reading it:

- **A channel voice message inside a sequence has to be built here.** A control's own rows leave in the pointer handler, so the plan builder skips them — and a sequence step went down the same path and sent nothing at all. Nothing is holding a step, so a step builds its own words.
- **A note step is on, wait, off, in one piece.** A list of steps is exactly where a hanging note is easy to forget, so the hold time is part of the step rather than something to remember to add.
- **A step kind this build does not understand does nothing and keeps its own name.** Falling through to the default was falling through to a control change, so a file written by a newer build would have sent controller 0 to somebody's desk.

## Learn — five things, not one

Touching a control on hardware says which endpoint it arrived on, which group, which channel, which kind of message and which number. Capturing only the number is why remapping a controller is usually an hour of typing.

- A check box per field decides what a capture may write, so somebody remapping inside one device can lock the endpoint and take only the number.
- **A note off, a control change at zero and pitch bend at center do not arm anything.** All three arrive constantly while somebody is still reaching for the control they actually mean.
- **A swept knob is one binding.** Without that, a bank learn would fill eight controls from one gesture.
