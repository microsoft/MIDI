# MIDI Glass — visual design proposal

Working name. A customizable MIDI control surface for Windows, in the Windows MIDI Services tool family. This document goes with the ten HTML mockups in this folder — serve them with `pwsh -File serve.ps1` and open `http://127.0.0.1:8742/`. Captured PNGs are in `shots\`.

Everything here is a proposal to argue with, not a plan. I have marked the places where I picked one of several reasonable answers, and the places where I think you should pick.

---

## 1. The idea in one paragraph

A customer drags controls onto a page, says what each one sends, saves it, and runs it full screen on a touch monitor or a tablet. The app itself looks like every other Windows MIDI Services tool — WinUI, Mica, the same title bar, the same appearance flyout. The surface the customer builds does not. It is a dark glass deck with colored light, and it is the only thing on screen when the layout is running.

---

## 2. The surface visual language

**Screen 9** (`9-language.html`) is the sheet: every control type at rest and in use.

The whole language is four ideas.

**The plate.** Every control sits on a dark, slightly translucent plate — about 86 % opaque over the deck, with a 7 px corner radius and no gradient, no bevel, no inner shadow. The translucency is the only skeuomorphic thing in the app and it is doing real work: it lets a themed or photographic deck show through faintly, so a surface reads as one object instead of a scatter of stickers. That is the "glass".

**The rim.** A one-pixel hairline in the control's hue, at low opacity when the control is idle. This is the entire resting identity of a control. Nothing is filled, nothing is saturated, nothing glows until it is doing something.

**The light pipe.** The value lives in a single bar of hue — an arc around a knob, a slot down a fader, a dot on a pad, a crosshair on an XY field. Same idea, same color, four shapes. Because the pipe is always the hue and the plate is always dark, you can read a forty-control page from across a room and know which faders are up without reading a single label.

**The bloom.** Touch or incoming activity lifts the rim to full and throws a soft glow of the same hue behind the plate. It decays over about 200 ms. Activity is the only thing in the whole language that blooms, so a surface tells you what is moving without any animation you have to interpret.

**The one rule that holds it together:** a control never uses more than one hue, and a hue never appears anywhere except the rim, the pipe and the bloom. That is what keeps a busy page readable, and it is what makes a theme swap a six-color operation instead of a redesign.

### Why this is not TouchOSC

TouchOSC and TouchDAW are flat fields of saturated color — the control *is* the color block, and when it is off it is a gray block. That reads well on a phone and badly on a 32-inch monitor, because at rest the whole surface is either shouting or dead.

MIDI Glass inverts it. At rest the surface is nearly black with thin colored outlines, which looks like hardware sitting under stage lighting. Under the hand it lights up. The reference points I took from your images are the Traktor-style deck (dark field, one hue per deck, value carried by a thin bright line) and the Reaktor 8 Steps panel (hairline outlines, everything on one accent, generous space between objects) — not the iPad Logic controller, which is the saturated-block style we are deliberately not doing.

### Typography

In-box fonts only, so a layout shared on a forum looks the same on the other person's PC. The customer gets a short list rather than a font picker: **Segoe UI Variable** (default, and the small-text one), **Segoe UI Variable Display** (headings on the surface), **Cascadia Mono** (numbers, readouts, anything that must not jump around as it changes), **Bahnschrift** (condensed, for narrow strip labels), and **Consolas**. Sizes are a scale, not a spinner: 9 / 11 / 14 / 18 / 24 / 32 / 48 / 64. Weight is regular or semibold. Nothing else.

### Measurements

Everything is a multiple of **4 px**, and the default grid is 8 px, so snapping always lands cleanly and a control never ends up on a half pixel. Default sizes are derived from the page template rather than fixed, so a 1024 × 768 page gets chunkier defaults than a 2560 × 1440 one: on a 1280 × 800 page a pad is 56 × 56, a knob 56, a fader 40 × 180, an XY pad 240 × 240. A page has a fixed pixel size chosen from a template and scales to the window, letterboxed — so a layout built on a laptop looks right on a projector, and a fader is never 3 px wide. See section 15.

---

## 3. The library — where a customer starts

**Screen 1** (`1-library.html`).

The app opens on a wall of cards, not a file dialog. Each card is a **thumbnail drawn from the layout model**, not a screen capture, so it is correct before the layout has ever been opened on this PC and it costs nothing to draw. Under it: name, the customer's own description, how many of its devices are ready, and when it was last used.

- **Favorites** first, as large cards. **Recent** below, as small ones. "See all" opens the full grid.
- Hovering a card, or touching it, reveals **Run** and **Edit** and a `...` menu. Run is the primary action and Enter runs it. Ctrl+Enter edits it.
- **New layout** is a card in the grid, and it offers templates — blank, mixer, DJ deck, drum pads, transport.
- A **missing device never blocks opening.** The card says "1 device missing", the layout still runs, and the controls that need it show as unavailable until it comes back. Nothing has to be repaired before a show.

Files live in **`Documents\MIDI Layouts`**, one `.midilayout.json` per layout, exactly the shape MIDI Patchbay uses for patches. Thumbnails are cached under `%LOCALAPPDATA%` and regenerated when the file changes, so the Documents folder stays clean. "Open a file…" and "Show in folder" exist for the day someone is handed a layout by a friend, and that is the only time a customer sees a path.

---

## 4. Running a layout

**Screen 2** (`2-run.html`) windowed, **screen 3** (`3-fullscreen.html`) full screen.

Windowed, the surface fills the window under the normal WinUI title bar and one thin bar: back to Library, Edit / Try, the page tabs, an output indicator, **Panic**, and full screen. The surface opens at its **designed size and scrolls**; **Fit to screen** and a custom percentage are on the same bar, and whichever was last used is saved with the layout. Full screen (**F11**) drops the bar entirely. The deck runs to every edge, and a toast fades in for about two and a half seconds — *Press **Esc** to leave full screen* — exactly like a browser. It never comes back unless the pointer leaves the window and returns.

- **Full screen carries no bar at all**, just one 34 px button in the corner the layout names, which fades back after a few seconds of no touch. Its flyout holds scale, pages, layout settings, leave full screen and Panic. Screen 14 shows it.
- While a layout runs full screen the app holds a **display-required** request, so a performer's screen does not blank mid-set.
- Which display it opens on is remembered per layout.
- Full screen is a presentation mode, not a different mode of the app. Nothing reconnects, nothing restarts, the surface keeps running through the switch.

**Panic** is always one press away in both modes: all notes off and all sound off on every group and channel of every device the layout uses, plus reset controllers. It is the one button a performer needs to find without looking.

---

## 5. The editor

**Screen 4** (`4-editor.html`).

Four regions: **palette** left, **page** center, **inspector** right, **message monitor** along the bottom, with the page rail between the canvas and the monitor.

**Edit and Try are one keystroke apart.** Ctrl+Enter flips the canvas live without leaving the editor, so a customer can feel a fader and then nudge it two pixels. That single toggle is the most important interaction in the app — every control surface editor I have used makes you leave the editor to find out whether the thing you built is comfortable, and by then you have forgotten what you were going to change.

**The monitor rail** shows what is actually going out, filtered to the selected control by default. It is the same message view the MIDI Monitor tool uses. It answers "is this thing sending what I think it is" in about one second, and it is the reason a customer will not have to open a second app.

**Nothing is modal.** The inspector always reflects the selection. With nothing selected it shows the page's own properties.

### The palette

Grouped and searchable, with a "Recently used" section that fills in as they work and a **My snippets** section at the top (**screen 5**). A snippet is any selection saved by name — a channel strip, a transport cluster, a whole eight-fader bank — and it reuses the same drag, so building a second mixer is a drag rather than a rebuild.

Two ways to place a control, because people expect different ones:

- **Drag** it onto the page. It snaps to the grid on the way in and shows a ghost plus a "drop here" pill.
- **Click** the palette entry, then click the page. Better with a pen or a finger, and it is the path a keyboard user can take.

### Positioning and resizing

- **Snap to grid** is on by default with a size picker (4 / 8 / 16 / 32 px, or off). **Alt suspends it** while dragging, so free-form placement is always available without changing a setting.
- **Magnetic guides.** Dragging near another control's edge, center line or the page margin snaps and draws a pink guide. Multiple guides at once, exactly as in a drawing app.
- **Spacing pills.** With three or more selected, the gaps show as small numbers between them, and typing in one sets them all equal. This is how "make these eight pads evenly spaced" happens without an Align menu.
- **Eight handles** on a selection, corner handles hold the aspect ratio, and a lock in the inspector fixes it permanently. A knob and a pad default to locked square.
- **Arrow keys nudge one pixel, Shift+arrow one grid cell.** The inspector always shows exact X, Y, width and height, and typing in them is exact.
- **No free rotation.** Deliberate: a rotated fader is hard to hit and impossible to describe to a screen reader. Text and images can be turned in 90° steps, which is all a vertical strip label needs.

### Repeat — the biggest time saver in the app

**Screen 5** (`5-arrange.html`), bottom half.

Building a sixteen-channel mixer by hand means sixteen copies and sixteen hand-edited channel numbers, and one of them will be wrong. Repeat takes the selection and asks three things: how many copies, which direction and gap, and **what should step** — channel, controller number, note number, group, or nothing. It fills the label from a `{n}` pattern at the same time. One strip in, a correct bank out. It works on any selection, so a whole channel strip repeats as a unit.

### Keyboard order

A badge on each control shows its tab position, and the badges can be dragged to reorder. This is also the order a screen reader walks, so it is not an accessibility afterthought bolted on later — it is a visible property of the layout that the person building it can see and fix.

---

## 6. What a control sends

**Screen 6** (`6-bindings.html`). This is the part every other control surface app gets wrong, so it gets the most careful screen.

A control has a **list of messages**, not one message. Each row says **when** (it turns on, it turns off, it changes, it is touched, it is released), **what** (note, control change, program change, pitch bend, channel pressure, per-note controller, registered or assigned controller, SysEx, a raw UMP, or a named sequence), and **to where** — device, group, channel, independently per message.

So a single button can send a note to the synth on group 1 channel 1, a note off to the same place when it turns off, and a control change to the DAW on channel 16, and that is the ordinary case rather than an advanced one.

**Destinations are named, not wired.** "Synth" and "DAW" are entries in the layout's own small output table (**screen 10**). A control points at the name. Moving a whole layout to different hardware is one edit in one place instead of a hunt through two hundred controls.

**Resolution is handled for the customer.** A value is stored once, as a fraction of full scale. If the endpoint speaks MIDI 2.0 it goes out at full 16- or 32-bit resolution; if it speaks MIDI 1.0 it is folded down to 7 bits on the way out. Nobody has to know which, and the same layout file works on both. The binding row shows the wire bytes for the current device so an expert can check, and says what it becomes on a MIDI 1.0 device.

**Learn.** One button, then touch a control on the hardware and the row fills in. The obvious feature, and it has to be one click.

**Feedback.** Any control can take an incoming binding, so a fader follows the DAW and a button lights from the device rather than from its own state. Meters, lamps and readouts are input-only controls built on the same mechanism.

**Pickup mode** is per control — jump, catch, or relative — because a page of faders that fight the DAW is worse than no page at all.

---

## 7. Sequences, and the scripting question

**Screen 7** (`7-sequence.html`).

A button can run a short **list of steps**: send a note, send a message, send system exclusive (typed, or from a `.syx` file), **wait N milliseconds**, set another control's value, jump to a page, hold a layer, or **repeat a block N times**. Steps can be dragged to reorder. The dialog shows the total time. "Test it" runs it without leaving the editor. Pressing the button can run it once, repeat it while held, or start it and stop it on the next press.

**Waits never block the surface.** Each running sequence is a small scheduled state machine on the MIDI clock, not a sleeping thread, so twenty buttons can be mid-sequence while a fader still moves at full rate.

**This should not be a new engine.** Section 12 works through what the shipped `Windows.Devices.Midi2.Utilities.Sequencing` already gives us, what it is missing, and the four small additive changes that would let MIDI Glass, the MIDI Player and a future performance-rig app all run on the same one.

### My recommendation on the JavaScript engine: don't. Not in this version, and probably not later.

You asked whether to bring in the Edge JS engine for complex layouts. I think the answer is no, for four reasons, and I would rather be argued out of it than have it in by default.

1. **Layouts get shared.** A layout file downloaded from a forum is untrusted input from a stranger. Putting a general-purpose script engine behind one turns a sequencer into a code delivery mechanism, and the first interesting thing someone builds with it will be the reason we have to pull the feature.
2. **It is a large runtime in a tool that has to start instantly and run with no network.** This app is the one that gets opened four minutes before a set.
3. **A fixed step list covers what people actually build.** Recall a patch, arpeggiate, stage a crossfade, jump to a page. I went looking for the cases that genuinely need loops and branches and could not find one that was not also a case for a real sequencer.
4. **Every step is inspectable, localizable and readable by a screen reader.** A script is none of those. The step list can be shown in the UI in the customer's language; a blob of JavaScript cannot.

If it ever does need more, the right escape hatch is a **tiny expression grammar for value mapping only** — `value * 0.5 + 0.25`, `1 - value`, a curve name — with no loops, no I/O and no file access. That is a few hundred lines, it covers the "I need a different fader taper" complaint, and it cannot be turned into a payload.

---

## 8. Themes

**Screen 8** (`8-theme.html`).

A theme is **six hue slots, a deck, and a handful of control defaults**. A control stores "slot 3", never `#FFC247`. Switch theme and every amber control becomes lime together, and the mute buttons still all match. A control can still override with a literal color when it has to, and the theme editor says how many controls are using each slot so a customer knows what a change will touch.

- Shipped themes: **Studio Dark**, **Neon Booth**, **Daylight**, **Amber Console**, **Blueprint**, **High contrast**, and the three below.
- Deck: one color, a two-stop gradient, or an image.
- Control defaults in the theme: corner rounding, glass tint percentage, glow strength, label placement. This is how a customer makes their whole surface squarer or flatter in one move.
- **Contrast is measured, not guessed.** Each slot is checked against the deck and flagged before it ships to a stage. In the mockup, slot 5 at 4.1 : 1 is called out with what to do about it.
- A theme is a small separate file, so one can be saved and reused across layouts, or shared.
- The **High contrast** theme is *offered, never forced*. If Windows switches to a high-contrast theme while a layout is running, the running layout is left alone and the offer waits until it is next opened. Reskinning a performer's surface mid-set would be worse than the problem it solves.

### Two tonal themes: Pigment Light and Pigment Dark

**Screen 11** (`11-tonal-themes.html`). Flat opaque plates instead of glass, and depth carried by a **tonal wash of the control's own hue** instead of a glow. A control at rest is filled rather than outlined, corners are rounder, and pressing it deepens the wash rather than lighting it up. The result reads friendlier and less like stage equipment, which suits a studio, a classroom or a desk in daylight.

**This costs no new rendering layers**, which is the only reason it is worth doing. The elevation idea these borrow is the newer one, where a raised surface is tinted with its own color rather than casting a shadow — and a tint is a background color. The older card-and-shadow look would mean a composition shadow under every control, which on a surface that has to stay smooth under a finger is a real cost for eighty controls. If that look is ever wanted, it should be a separate conversation with that number attached.

They do need two things the theme model does not have yet:

- **Fill at rest** — outlined or tonal. One property, and it is what makes these read as a different family rather than a recolor.
- **A track color** for the empty part of a fader slot and a knob arc. The dark themes get away with hardcoding this as black. That is a gap in the theme model rather than a cost of these themes, and it would have bitten the first customer who built a light theme of their own.

The honest trade: these break the "nothing is saturated at rest" rule that keeps a busy page readable. A tonal page is colorful when nothing is happening, so activity has less room to stand out. On a forty-control mixer that is a fair trade. On a hundred-and-twenty-control page Studio Dark still wins, and the theme picker should say so rather than letting somebody find out during a set.

Names are placeholders. **Studio Paper** and **Studio Slate** would sit closer to the existing family if they should read as siblings of Studio Dark.

### Bigwig

**Screen 12** (`12-bigwig.html`). Raised mid-gray panels on a near-black deck, one strong orange doing all the work, and the value strip running along the *top* edge of a control the way a channel header does. It uses the tonal machinery with the hue wash turned **off**, so the plate stays a neutral gray and orange only ever means "this is the value" or "this is on". That restraint is what makes a dense page readable, and it is a third point on the same scale rather than a new mechanism.

**The segmented LED ring is worth doing.** A ringed knob is still *one* element, exactly like the solid arc it replaces, because the lamps are a repeating mask laid over the arc rather than thirty separate shapes. Lamp count is one number in the theme and does not change with size. The catch is size, not count: below about 48 px the lamps stop separating and the ring reads as a fine comb — still legible as a value, but no longer the effect. A knob that small should fall back to the solid arc on its own rather than making somebody notice and fix it.

**Deferred:** drop shadows. The older card-and-shadow look would mean a composition shadow under every control, which on a surface that has to stay smooth under a finger is a real cost at eighty controls. Not doing it for now. The second arc of white lamps on the hardware is skipped too — that is an endless encoder with no pointer, so the lamps are showing what the main arc is not, which our knobs do not need.

### The theme model, consolidated

Three themes past the original six have each asked for one small thing, and together they settle what a theme actually is. This is the list to build against.

| Property | Values | Added for |
| --- | --- | --- |
| Six hue slots | color each | original |
| Deck | color, gradient or image | original |
| Corner rounding | px | original |
| Glass tint | 0 (opaque) to 100 (today's glass) | original |
| Glow strength | 0 to 100 | original |
| Label placement | inside, below, none | original |
| **Fill at rest** | 0 to 1, a wash of the control's own hue | Pigment |
| **Track color** | color for the empty part of a slot or arc | Pigment |
| **Rim source** | the control's hue, a neutral edge, or none | Bigwig |
| **Value strip** | bottom, top, or none | Bigwig |
| **Value indicator** | solid arc, or segmented with a lamp count and a minimum size | Bigwig |
| ~~Elevation shadow~~ | deferred, real cost at scale | — |

The track color is worth calling out separately: it is not a cost of any of these themes, it is a gap in the original model. The dark themes get away with hardcoding it as black, and the first customer who built a light theme of their own would have hit it.

---

## 9. Pages, layers and devices

**Screen 10** (`10-pages-devices.html`).

**Pages** are the tab control you asked for, but the tab strip is a surface object drawn in the theme, not a WinUI `TabView` sitting on top of the deck. Pages are reordered by dragging, each has its own hue, and a control can switch pages as one of its messages — so a page change can be a pad on the surface rather than a tab at the top.

**A shared band** can be pinned to every page, for transport and panic, so they do not have to be rebuilt four times.

**Layers** are the modifier idea: a layer key swaps the bindings on the controls that have an alternative, while held or latched on a double tap. Controls that change get a thin second rim in the layer's hue, so you can see what Shift will do before you press it. This is how one page of eight pads becomes three kits.

**The device table** is what makes a layout portable. Each entry has a friendly name the controls point at, and a **match rule** — exact device id, name, USB model, or serial number. Those are the same criteria the service configuration and MIDI Patchbay already use (`MidiServiceConfigEndpointMatchCriteria`), so this is not new matching code and a layout built on one PC has a real chance of finding the right hardware on another. Exact device id is the default because it is unambiguous; the looser rules exist for the day someone plugs the same keyboard into a different port.

A device that is not here does not block anything. The entry says so, the controls that use it stay dark, and the moment it appears they come back on their own. Repointing it at different hardware is one click and every control that used it follows.

**Connections are the app's problem, not the customer's.** One connection per device, opened when the layout loads, shared by every control and every page, closed when the layout closes. Group and channel travel inside the message, so hundreds of controls spread over four groups of one device still cost exactly one connection. Feedback comes back on the same connection.

---

## 10. Accessibility

This is a graphical surface, which makes it easy to build something unusable, so it is worth stating up front what the answers are.

- Every control has a name, a role and a value in UI Automation. A fader is a slider with a range, a toggle is a toggle button, a pad bank is a group of buttons.
- **Keyboard order is editable and visible** (screen 5), and it is the order assistive technology follows.
- Every control is operable from the keyboard: Tab to it, Space or Enter for a button, arrows for a continuous control, Home and End for the ends.
- The contrast of every theme slot against its deck is measured in the theme editor and flagged before it ships.
- Labels are real text in the layout file, so a screen reader reads a fader as "Channel 3 volume, 74 percent", not "slider".
- **Accessibility check** is a page in Layout settings that lists unnamed controls, low-contrast slots and controls that cannot be reached by keyboard, before the customer shares the file.

---

## 11. Where this fits in the family

Nothing here is a new pattern — the point is that it is the same app family.

| Comes from | What MIDI Glass uses it for |
| --- | --- |
| `midi-app-shared` `WindowChrome`, `MidiAppSettings`, `ShowAppearanceFlyout` | Title bar, Mica, theme, always on top, window placement, the gear in the title bar |
| `SingleInstance` | One copy; launching again brings the running one forward |
| `MidiServiceConfigEndpointMatchCriteria` | The device table's match rules — no new matching code, no new API surface |
| MIDI Patchbay's document model | One JSON file per document in Documents, auto-save with a "Saved / Not saved" chip, description on the card |
| `BeatClockGenerator` in `midi-app-shared` | The beat clock control, and the tempo source for LFOs and sequences |
| `GeneralMidi` in `midi-app-shared` | Program names in the binding editor |
| MIDI Monitor's message view | The monitor rail at the bottom of the editor |
| `EndpointImageAssets` + the Win32 common item dialog | Deck images and layout export — **never `Windows.Storage.Pickers`** |
| The `PREVIEW` chiclet | Title bar badge while this is a preview |

Proposed identity: executable `midiglass`, title bar **Windows MIDI Glass**, Start menu **MIDI Glass**, settings under `HKCU\Software\Microsoft\Windows MIDI Services\Tools\midiglass`, documents in `Documents\MIDI Layouts` as `*.midilayout.json`.

---

## 12. Reusing the sequencing API — can MIDI Glass build on what already ships?

You asked whether the sequence engine can be shared with other first-party apps, and whether it can leverage `Windows.Devices.Midi2.Utilities.Sequencing`. Short answer: **about half of it, today.** The hard half — the scheduling — is already there and is worth building on. The other half is missing in a way that also blocks the file player's own roadmap, so it is worth fixing regardless of this app.

### What is already reusable, and it is the part that is hard to get right

`PlaybackEngine` hands every message to the service stamped with the time it is due and lets the service release it, sweeping a 250 ms look-ahead window every 40 ms, rebasing the clock on seek, chasing bank/program/controller state, and firing a full panic on stop, pause, seek and end. That discipline is the whole reason playback is steady, it is tested (65 of 65 in the TAEF suite), and nothing about it is specific to files. **MIDI Glass should not write a second one of these.**

### Four things block reuse

**1. A sequence can only come from a file.** `MidiSequence` has no constructor and no builder. The only thing in the whole projected surface that produces one is `MidiStandardFileReader`. A button that plays six notes has nothing to hand to the player.

**2. Events are stored as MIDI 1.0 bytes.** `SequenceEvent` is a byte range in a MIDI 1.0 blob, and `PlaybackEngine::Load` runs every event through `ConvertMidi1CompleteMessageBytesToUmpWords`. So a sequence cannot express a native MIDI 2.0 message at all — no 32-bit controller, no 16-bit velocity, no per-note controller, no registered or assigned controller, no UMP stream message. That is a bigger problem than the control surface case: it is also why the SMF-2 clip reader has nowhere to put its output.

**3. Time is musical only.** Everything is ticks against a tempo map. *Wait 250 milliseconds* — the step you asked for — has no representation, and neither does a SysEx dump that needs a real delay between packets regardless of tempo. That is not hypothetical; the Marshall firmware work needed exactly that.

**4. One player is one connection and one group.** `MidiSequenceTrackRouting` exposes `Connection`, `Group` and `ChannelOverride`, but `SetTrackRouting` only applies `IsMuted` — the other three are stored and never read. The engine holds a single `m_connection` and a single `m_groupIndex`. **This one is a defect, not a gap:** the shipped IDL comment and the published page both say "tracks may be sent to different endpoints at the same time and still stay together... that is what makes it reasonable to drive a rack of hardware from one file", and that does not work today. Driving a rack from one document is the ordinary case in a MainStage-type app, not an advanced one.

### Four changes, all additive, none of which disturb the file player

**Change A — honor the routing that is already declared.** Give `PlaybackEngine` a small destination table instead of one connection, put a destination index on `PreparedEvent`, and have `SendWordsUnderLock` pick by index. Because every message is scheduled against the same `MidiClock`, tracks on different endpoints stay together for free — the promise is true of the design, it was just never wired up. **No API surface changes at all**, which is a good argument for doing it first.

**Change B — let a sequence carry UMP natively.** One flag on `SequenceEvent` saying the bytes are already UMP words, and a branch in `Load` that copies them through instead of converting. The SMF reader never sets it, so every existing file behaves identically. The group nibble in a stored packet is rewritten to the destination group on the way out, the same rewrite `MidiMessageForwarder` already does. This single flag unblocks MIDI 2.0 playback, the clip reader, and a fader sending full-resolution values.

**Change C — `MidiSequenceBuilder`.** A new runtime class in `Utilities.Sequencing` that produces the same `MidiSequence` the reader produces, so the player, the piano roll and every position map work unchanged. The native model already supports this — every field on `midifile::MidiSequence` is public and `Finalize()` builds the derived maps — so this is a projection over something that exists rather than new engine work.

```
runtimeclass MidiSequenceBuilder
{
    MidiSequenceBuilder();
    UInt16 TicksPerQuarterNote;
    UInt16 AddTrack(String name);
    void AddTempoChange(UInt32 tick, Double beatsPerMinute);
    void AddTimeSignature(UInt32 tick, UInt8 numerator, UInt8 denominator);
    void AddMessages(UInt16 track, UInt32 tick, UInt32[] words);
    void AddSystemExclusive(UInt16 track, UInt32 tick, UInt8[] data);
    void AddNote(UInt16 track, UInt32 tick, UInt32 durationTicks, MidiChannel channel, UInt8 noteNumber, UInt16 velocity);
    MidiSequence GetSequence();
}
```

`AddNote` writing both halves and the paired note at once is the point of it — a hand-built sequence with a hanging note is the most common way this goes wrong, and a builder that cannot produce one removes the whole class of bug. The builder also pays for itself well outside this app: it is what an SMF *writer* needs, what a "capture what I just played" feature needs, and what lets the test suite build a fixture without a file on disk.

**Change D — wall-clock timing beside musical timing.** A `TimingMode` of `Musical` or `Absolute` on the sequence, or an absolute-microsecond stamp on an event. `Load` already places every event through `MicrosecondsAtTick`, so it is one branch there. This is what makes a Pause step real.

### What I would not do

**Do not add a live or real-time mode to `MidiSequencePlayer`.** With A through D, MIDI Glass's button sequences are just short sequences: build once, `SetSequenceAsync`, `Play()`. Adding a second execution model to a player that already works would cost more than it returns.

**Do not re-add `LookAheadMilliseconds`.** It was cut deliberately and the reasoning is written into the IDL. Nothing here changes it.

### The one thing I cannot decide on my own — scale

Twenty buttons mid-sequence means twenty `MidiSequencePlayer` objects, each with its own worker thread sweeping every 40 ms. It works, but twenty threads to send a handful of messages is wasteful, and on a tablet it will show.

Either MIDI Glass keeps a small scheduler of its own for short sequences and uses the API only for long ones — which is the duplication we are trying to avoid — or the sequencing implementation gains **one shared sweep that services every open player**. I would take the shared sweep. It moves no API surface at all; `PlaybackEngine` takes its tick from a shared source instead of owning `m_worker`. The right moment is when the builder lands, because that is when "many small players" stops being an unusual way to use the API.

### Suggested order

| | Change | Surface | Why here |
| --- | --- | --- | --- |
| 1 | A — routing | none | A defect against shipped and published text, smallest diff, useful alone |
| 2 | B — UMP events | none (internal flag) | Unblocks the clip reader and MIDI 2.0 playback |
| 3 | C — builder | contract 2 | The thing MIDI Glass actually needs |
| 4 | D — wall clock | contract 2 | Ship with C |
| 5 | Shared sweep | none | Once C makes many players normal |

**One question for you before any of this is written.** The model and the engine live in `src/in-box/Inc`, which the servicing rules list as shipping. But the only consumers are the SDK, its unit tests and the player app — not midisrv, not a driver, not a transform — and you have said changes inside the SDK do not need a gate yet. Does the `Inc` location decide it, or the consumer set? I would rather ask than guess.

---

## 13. The name — settled

**MIDI Glass.** Executable `midiglass`, title bar **Windows MIDI Glass**, Start menu **MIDI Glass**. The theme names **Pigment Light**, **Pigment Dark** and **Bigwig** stay too.

---

## 14. Decisions

Everything that was an open question is now answered. Recorded here so the reasoning does not have to be reconstructed later.

**Page size is fixed, and scaling is letterboxed — never stretched.** A new layout starts from a **page size template** (screen 13): 1280 × 800, 1920 × 1080, 2560 × 1440, 2736 × 1824, 1024 × 768, 1080 × 1920 portrait, or custom. On a machine that is not the one it was drawn on, it scales 1:1 on both axes and letterboxes. A control surface is muscle memory; a control that changes shape between sessions is worse than a black bar.

**The editor works on a virtual canvas bigger than the page** (screen 13). Growing the page asks where the existing controls should sit, using a nine-position anchor. Shrinking offers to scale everything down, or to leave it and flag what falls outside. Controls outside the page are **ghosted with a red dashed edge**, counted in a warning strip, and stay fully selectable and draggable. The alternative — clamping everything inside — sounds tidy and is miserable: shrink a page and eleven controls silently pile up along the right edge with no way back. This way a resize is always reversible.

**At run time a layout opens at its designed size and scrolls** (screen 14). **Fit to screen** is one press and scales 1:1 both axes. A custom percentage is available. Whichever of the three the layout was last left in is what it opens in next time. Actual size is the default because it is the only mode where a control is exactly the size the person who built it intended.

**No scripting engine.** Settled. The step list in section 7 is the answer.

**MIDI Glass presents itself to other apps as a virtual device**, not as a loopback pair. A virtual device's lifetime is tied to the session that creates it, which is exactly how this app already manages connections — the device appears in a DAW while MIDI Glass is running and goes away when it is not, with no machine configuration left behind. A loopback would persist in the service configuration after the app closed and would need cleaning up. This is optional per layout, off unless asked for.

**The device table is per layout**, so a layout stays portable. The match logic that MIDI Patchbay already has is **moved into the shared library** so both apps use one copy.

**Touch target size is the customer's call, with defaults that scale to the page.** They know how big their fingers are. What the app owes them is a sensible starting size derived from the page template — a 1024 × 768 page gets chunkier defaults than a 2560 × 1440 one — and the accessibility check to tell them when something got too small.

**A layout always starts from its own default values.** Runtime values are never carried between runs, because they will not be right. Each control can be marked to **send its value as an initialization step** when the layout starts, and there is a global override to suppress all of it — a layout whose faders are at zero, pushed to a live desk on load, mutes the show.

---

## 15. The canvas and the run-time view

**Screen 13** covers the page size templates, the virtual canvas, off-page controls and the two resize paths. **Screen 14** covers the three scale modes and the full screen chrome.

**Full screen carries no bar.** The deck runs to all four edges. The only chrome is one 34 px button in **whichever corner the layout names**, so it can be put where that particular rig has nothing important. It fades to about a quarter opacity a few seconds after the last touch. Its flyout holds scale, the page list, "move this button", layout settings, leave full screen, and **Panic — always at the bottom, always the same color, never moved**, because muscle memory matters more than tidiness for that one. Panic is on a hardware key as well.

---

## Files

| File | Screen |
| --- | --- |
| `index.html` | Contents page for the mockups |
| `1-library.html` | Library — favorites, recents, thumbnails, Run and Edit |
| `2-run.html` | Running a layout, windowed |
| `3-fullscreen.html` | Full screen, the Esc toast, a second theme. Its auto-hide bar is superseded by the corner button on screen 14 |
| `4-editor.html` | The editor — palette, page, inspector, monitor rail |
| `5-arrange.html` | Snapping, spacing, Repeat, keyboard order |
| `6-bindings.html` | What a control sends — three messages, two devices |
| `7-sequence.html` | Sequences — SysEx, notes, waits, repeat blocks |
| `8-theme.html` | Themes — six hue slots, deck, measured contrast |
| `9-language.html` | The surface control language — every control type |
| `10-pages-devices.html` | Pages, layers, and the device table |
| `11-tonal-themes.html` | Pigment Light and Pigment Dark — the tonal theme family |
| `12-bigwig.html` | Bigwig — gray panels, one orange, and the segmented LED ring |
| `13-canvas.html` | Page size templates, the virtual canvas, off-page controls, resizing |
| `14-running-scale.html` | The three scale modes, and the full screen corner button |
| `mock.css` | Shared styles for all fourteen |
| `serve.ps1` | Local static server on port 8742 |
| `shots\` | PNG captures of all fourteen screens |
| `MIDI-Glass-implementation-plan.md` | Remaining design gaps, the engine layering, the API work, and the phases |

These live in `src/prototypes/midi-glass/design/` so the design record is versioned. Nothing in this folder ships; when the app is real it goes to `src/in-box/user-tools/midi-glass/` like every other tool.
