# MIDI Glass — implementation plan

Companion to `MIDI-Glass-design.md` and the twelve mockup screens. This is the *how*, sequenced.

---

## Where this stands — read this first

**As of 24 September 2026. Phases 0, 1, 2, 3, 4 and 5 are done. Phase 6 has not started.**

| Phase | State | Where the code is |
|---|---|---|
| 0 Spikes | **Done** | `src/prototypes/midi-glass/spikes/`, findings in `MIDI-Glass-phase-0-findings.md` |
| 1 SDK work | **Done** | `src/in-box/Inc/midi_sequence_*`, `src/in-box/Client/WinRT/core/MidiSequence*` |
| 2 Shell | **Done** | `src/in-box/user-tools/midi-glass/` |
| 3 Document, themes, thumbnails | **Done** | `midi-glass/document/`, `midi-glass/thumbnail/` |
| 4 Runtime surface | **Done** | `midi-glass/binding/`, `runtime/`, `surface/`, `RuntimeWindow.*` |
| 5 Editor | **Done** | `midi-glass/editor/`, `EditorWindow.*`, `EditorCanvas/Inspector/Outline/Dialogs/TryMode.cpp` |
| 6–9 | Not started | — |

**Tests: 243, all passing, none needing a window or a device.** `src/in-box/Test/Tools/Midi2.MidiGlass.unittests`. Builds clean x64 Release; spelling and accessibility checks clean.

```
build  msbuild <proj> /t:Build /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=<repo>\src\in-box\\" /v:minimal /nologo /nodeReuse:false
run    "C:\Program Files (x86)\Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe" <out>\tests\x64\Release\Midi2.MidiGlass.unittests.dll /logOutput:Low
spell  pwsh build\check_en_us_spelling.ps1 -Path src\in-box\user-tools\midi-glass
card   midiglass --thumbnail <layout.midilayout.json> <out.png> [width]
run    midiglass --run "<layout.midilayout.json>"
```

### What the app does today

Launch it and the **library** lists every layout in `Documents\MIDI Layouts`, each with a card drawn from the layout itself rather than captured from a window. It matches mockup screen 1: a toolbar with New layout, search, sort and a grid/list toggle; Favorites and Recent sections; a hover bar with Run, Edit and a context menu on every card; and a status bar that names the folder and reports whether the MIDI service is running. **New layout** asks which device to send to and offers five templates. **Run** opens a runtime window: the surface, the three scale modes, a device status line, View mode, full screen and Panic.

**Edit** opens the editor: the palette, the outline, the canvas with its work area and page, the inspector, snapping with magnetic guides, Arrange, Repeat, undo and redo, page resizing with the grow and shrink paths, auto-save with the Saved chip, and **Edit / Try on Ctrl+Enter with the monitor rail underneath**.

Not there yet: sequences, generators, learn, the per-layout virtual device, and the full screen corner button. Everything in that list is phase 6 or later.

### Two things learned here that apply to every tool in the family

- **A `Border` with a `CornerRadius` is aliased at fractional display scaling.** A `Border` is laid out, so its bounds and its radius are both snapped to whole pixels and the corner renders as steps at 125%. A `Rectangle` with `RadiusX`/`RadiusY` and `UseLayoutRounding="False"` goes through the shape rasterizer and stays smooth. Every pill and rounded outline in this app is a shape, and the `PREVIEW` badge in all thirteen in-box apps was converted to match.
- **`MidiApi::EnsureServiceAvailable()` starts the service**, so it can never back a status display — asking the question changes the answer. `midiapp::IsMidiServiceRunning()` in `midi-app-shared` is the read-only answer, and the library both polls it and re-checks it whenever a device arrives or leaves.

### Things that will bite, learned the hard way

- **`GetObject` and `SendMessage` are windows.h macros.** They break `IJsonValue::GetObject()` and any enum value named `SendMessage`. `#undef` right after the include, or do not use the name.
- **The app has a plain C++ namespace called `midiglass` as well as the projected WinRT one.** Inside `namespace glass`, `midiglass::GlassControl` resolves to the C++ one and fails. `surface/SurfaceRenderer.h` declares `namespace projected = ::winrt::midiglass;` and an alias for the control type; use them.
- **The service does the downscaling, both protocol and byte format.** Never fold to MIDI 1.0 in this app. See phase 4 below for the whole story.
- **A UMP monitor cannot see the MIDI 1.0 translation** — both ends are UMP. Use `midi1monitor.exe <portIndex>`, and **do not run it in a background terminal**; it is full-screen interactive and will lock the terminal.
- **Two agents cannot build the SDK at once.** Check `Get-Process MSBuild,cl,midlrt,midl,link` first and wait rather than killing.
- The document and binding layers, and the pure parts of the runtime and surface layers, are **free of `pch.h`, XAML and the MIDI SDK** on purpose, so the unit tests compile them unchanged. Both the app and the test project mark them `PrecompiledHeader NotUsing`. Keep it that way.

---

## 0. What this plan takes as settled, and what it does not

**Settled**, because the mockups and the theme work proved them out, and because the design questions have now been answered: the surface visual language; the control catalog; the theme model including the three properties the new themes exposed; the library-first customer journey; per-layout JSON in `Documents\MIDI Layouts`; named destinations with match rules; no scripting engine; fixed page size from a template with letterboxed scaling; the virtual canvas; one process with several windows; a virtual device rather than a loopback.

**Not settled, and deliberately deferred to a measurement rather than a document**: how a control is actually painted. That is the single biggest technical unknown in the project and it decides the shape of two engines. It gets a spike before anything is committed. See phase 0.

> **Phase 0 is done, 23 September 2026.** The answer is the **hybrid**, and the latency budget is written down. Numbers, method and the list of what was not tested are in [MIDI-Glass-phase-0-findings.md](MIDI-Glass-phase-0-findings.md). Everything below that describes phase 0 as open is history; the phase 0 section records what it actually found.

**Still open, but none of it blocks starting**: the three items in Part G.

---

## Part A — remaining design gaps, and what I propose

### A1. Windows, displays and instances

**The gap.** The rest of the tool family is single-instance with one window. MIDI Glass has a real reason not to be: a performer with a touch monitor beside a laptop wants a mixer running on one and a clip page on the other, at the same time.

**Proposal: one process, several windows.** Keep `SingleInstance` exactly as the family uses it, so launching again activates what is running rather than starting a second copy. Inside that process, the Library is one window and each running layout is its own window. `midiglass --run "Live Rig"` opens another runtime window in the existing process.

This is not just tidiness. One process means **one connection table**, so two layouts pointing at the same synth share a connection instead of fighting over one, and Panic can mean *everything this app is driving*. Two processes would give us two of everything and no way to reconcile them.

- Which display a layout opens on is remembered per layout, alongside window placement.
- If that display is gone, open on the primary rather than off-screen. Say so in a toast, once.
- A display change while running never moves a running layout. Moving a performer's surface mid-set is worse than leaving it where it is.
- The editor is one window at a time. Editing the same layout in two windows is a conflict nobody needs; the second request focuses the first.

### A2. The canvas, fit and zoom — DECIDED

**Page size comes from a template**, and the editor works on a **virtual canvas larger than the page**. Screens 13 and 14 have the detail; the work items are:

- A page size template picker on New layout, including portrait and custom.
- A work area around the page, with the page drawn as the bright active rectangle and everything else visibly not-shipping.
- **Grow**: a nine-position anchor for where existing controls land in the new space, with a live preview.
- **Shrink**: offer *scale everything to fit* or *leave it where it is*, naming how many controls would end up outside.
- **Off-page controls are ghosted with a red dashed edge**, counted in a warning strip, and remain selectable, draggable and reachable by *Select them* / *Bring inside*. Never clamped, because clamping makes a resize unrecoverable.

**At run time**: **Actual size with scrolling is the default**, *Fit to screen* is one press and scales 1:1 on both axes, and a custom percentage is available. The last used mode is saved per layout.

**Manual zoom is still modal** and this has not changed: a pinch on a control surface is ambiguous, because two fingers might be two fingers on two faders, which is the entire point of the product. Pan and free zoom live behind an explicit **View mode**, and **while View mode is on the surface sends nothing**.

### A3. Editing gaps

**Undo and redo.** Not in the design document and absolutely required. A layout edit is a document edit: one undo stack per open layout, coalescing a drag into a single entry, cleared on load, never crossing a save. Ctrl+Z / Ctrl+Y, and a visible depth in the Edit menu so it is obvious it exists.

**Keyboard-only editing.** Arrow nudge and typed X/Y/W/H already cover placement. What is missing is *selection* without a pointer: Tab walks controls in the layout's own keyboard order, Ctrl+Tab walks groups, and Enter opens the inspector focused on the first field.

**An Outline pane.** A tree of pages → groups → controls, selectable, reorderable, with the same context menu as the canvas. This is the answer to "how does a screen reader user build a layout", and it is also just useful on a 200-control page where clicking the right thing is hard. It is not an accessibility bolt-on; it is a feature that happens to solve the accessibility problem.

**Templates and snippets** are in the design already. What is missing is where a snippet lives — proposal: `Documents\MIDI Layouts\Snippets`, same file shape, and they show in the palette.

### A4. Learn — and it should do more than the controller number

You called this out and you are right that it is bigger than it looks. Touching a control on hardware tells us **five** things, not one: which endpoint it arrived on, which group, which channel, which message type, and which controller or note number. Today's plan only captures the last two.

**Proposal: Learn fills in the whole destination by default,** with a row of toggles for what to accept — endpoint, group, channel, message, number — so somebody remapping within one device can lock the endpoint and only take the number.

Three modes, because they solve different problems:

- **Learn one.** Arm, wiggle, done. The common case.
- **Learn a bank.** Arm, then touch eight knobs in order, and it fills eight controls in keyboard order. This is how somebody mirrors a hardware controller in under a minute instead of over an hour. **In scope for the first release.**
- **Learn feedback.** Arm, make the DAW send something, and the incoming binding is captured. The reverse direction, which nothing else in this space does well.

**Learn is design time only.** Learn-while-running is deferred — design time and run time should not mix until there is a proven gap that needs it.

### A5. Runtime safety

**A layout always starts from its own default values.** Runtime values are never remembered between runs, because they will not be right by the time the layout opens again. Instead, **each control can be marked to send its value as an initialization step** when the layout starts, in keyboard order, with a **global override to suppress all initialization**. That covers the case a layout genuinely needs — putting a synth into a known state — without the case that mutes a show, which is a layout whose faders sat at zero being pushed to a live desk on load.

**Device loss mid-performance.** Controls bound to a missing endpoint go unavailable and *stop sending*, rather than throwing. The layout keeps running. On return, the connection reopens and the controls come back. Nothing is queued while it is gone — stale MIDI arriving late is worse than nothing.

**Panic is in the chrome, not on the surface**, so it cannot be themed into invisibility or covered by a page change. All notes off, all sound off, sustain off, pitch bend center, on every group and channel the process is driving. In full screen the chrome is a single 34 px corner button whose corner the layout chooses; Panic is the last item in its flyout, always the same color, and never moves.

**An imported layout is untrusted input.** A layout file is data, but a sequence step can hold arbitrary system exclusive, and arbitrary SysEx sent to the wrong device can do real damage to it. So: a layout opened from outside `Documents\MIDI Layouts` is marked as imported, and the first time it is run the app says plainly which devices it will send system exclusive to and asks. Once. This is the one genuine security boundary in the app and it deserves a deliberate answer rather than silence.

### A6. Accessibility, beyond what the design document says

Four gaps I did not cover:

- **Reduced motion.** If Windows says reduce motion, the bloom stops decaying and starts switching, the LFO preview stops animating, and the beat clock pulse becomes a state change rather than a throb. The surface is a wall of animation by design; honoring this is not optional.
- **The canvas is not reachable by a screen reader.** The Outline pane above is the answer.
- **Touch target size is the customer's call.** They know how big their fingers are. What the app owes them is **defaults derived from the page template** — a 1024 × 768 page gets chunkier starting sizes than a 2560 × 1440 one — and an accessibility check that says when something ended up too small to hit.
- **Automation for custom-drawn controls.** If the rendering spike lands on custom drawing, every control needs a hand-written automation peer with the right pattern — slider with a range, toggle button, button. That is real work and it is why the spike has to consider it, not just frames per second.

### A7. Timing, tempo and clock

**Whose tempo?** A layout has one tempo source: internal, or follow incoming MIDI clock on a named input. Generators (beat clock, LFO, sequence steps expressed in note values) all read it. If the source disappears, hold the last tempo rather than jumping to a default.

**Sending clock out** is a per-destination switch, not a global one, because clock to a device that does not want it is noise.

### A8. File format and living with it

- **Version field, and a forward rule.** An older build opening a newer file keeps what it does not understand and round-trips it, rather than silently dropping it. Say clearly in the UI that it contains something this version cannot edit.
- **Auto-save with the Patchbay chip** — "Saved" / "Not saved", written about a second and a half after the last edit. Proven pattern, reuse it.
- **Thumbnails** cached under `%LOCALAPPDATA%`, regenerated on file change. **Not a permissions thing** — Documents is perfectly writable. The reasons are that the customer *looks* at their Documents folder and derived files clutter it, that a cache should not be synced to OneDrive or backed up, and that zipping `MIDI Layouts` to send to a friend should contain layouts and nothing else. Deleting the whole cache must never lose anything.

---

## Part B — the architecture

Layered the way MIDI Patchbay is, because that worked and because it is what lets a piece be lifted later. **Nothing below the window layer references XAML.**

```
LayoutModel        the document: pages, controls, bindings, sequences, devices. Pure data + validation.
LayoutStore        one .midilayout.json per layout, auto-save, thumbnails, import marking.
ThemeModel/Store   the theme property table, contrast measurement, built-in themes.
DeviceCatalog      watcher, match resolution, present/absent, reuses MidiServiceConfigEndpointMatchCriteria.
OutputRouter       named destination -> one connection per endpoint per PROCESS. Send and feedback.
BindingEngine      control value -> messages. Resolution folding. Feedback in. Allocation-free on the hot path.
SequenceRunner     step lists, on top of Utilities.Sequencing.
GeneratorEngine    beat clock and LFOs, on top of BeatClockGenerator. One tick source for the process.
LearnService       arm, capture, distribute across a bank.
SurfaceRenderer    draws a page. The performance-critical piece.
InputRouter        pointer/pen/touch/keyboard -> control -> BindingEngine. Multi-touch, one finger per control.
EditorController   selection, drag, snap, guides, repeat, undo.
MainWindow*        Library, Editor, Runtime windows. XAML only at this layer.
```

### Rules that keep this maintainable, and comprehensible to whoever picks it up next

These are worth writing down because they are the things that decay first.

1. **One folder per engine, with a `README.md` that states its contract, its invariants and — explicitly — what it does not do.** The "does not do" paragraph is what stops the next person, or the next agent, inventing capability that was never there.
2. **No engine calls up into the UI.** Engines raise events or return values. The window layer marshals.
3. **One class per file, and a file that passes about 600 lines is a signal to split.** `MainWindow` is already split by concern in three other tools in this family; do it from the start here.
4. **The hot path is named and documented in one place.** See below.
5. **Anything that will be projected later keeps natural names now** (`glass::LayoutModel`), and only gets the `Midi` prefix if it becomes API. Same rule the sequencing work used.
6. **Every engine gets a pure test** that does not need a window or a device. `BindingEngine` and `LayoutModel` especially — a compiled-against-the-real-source harness like the Patchbay one, not a mock.

### The hot path, stated once

From a finger touching glass to a message leaving the process there is **one frame of budget and no allocations**.

- The message is built and sent **in the pointer event handler**, not queued to a render tick. A render tick is up to 16 ms of latency for nothing.
- `BindingEngine::Evaluate` writes into a caller-owned buffer. No vectors, no strings, no map lookups by name — destinations resolve to an index when the layout loads.
- The surface repaint that follows is a separate, lower-priority concern. If a frame is dropped, the MIDI already went.
- Feedback in from a device is the mirror: parse, look up by index, set the value, mark dirty, let the next frame draw it.

### Value throttling on continuous controls

A fader dragged quickly can generate a message per pointer move. A high-rate digitizer delivers those faster than a DIN cable can carry them — 31250 baud is roughly 350 three-byte messages a second, shared with everything else on that wire. So a continuous control needs a rate limit.

**The throttle belongs on the value-changed notification, not in the engine.** One rate limit at the source, and both the send and the repaint fall out of it. That keeps it in one place, keeps the two in step, and means there is no way for the surface to show a value that was never sent.

Three rules make it safe:

1. **The last value is always sent.** A trailing send fires when the gesture ends, whatever the rate limit did, or a fader settles a few units away from where the finger left it. This is the part that gets forgotten and it is the part that matters.
2. **It is per control, and it is a number the customer can see.** A slider in the inspector, defaulting to something generous, with the obvious preset being "this one goes to a DIN device".
3. **Nothing else is throttled.** Buttons, notes, sequences and system exclusive go immediately. Rate limiting a note on would be a defect, not a feature.

### What is genuinely reusable, and what I would not generalize yet

**Reusable now, and should be built that way:** `OutputRouter` (named destinations over shared connections is exactly what a performance-rig app needs), `DeviceCatalog`, `SequenceRunner`, `ThemeModel`.

**Not yet:** `SurfaceRenderer`, `EditorController`, `LayoutModel`. They have exactly one consumer. Keeping them UI-free and well-bounded is enough; designing a second consumer that does not exist would cost more than it returns. The lesson from Patchbay is that structuring for a later lift works; *building* the lift early does not.

---

## Part C — the WinRT API work

Section 12 of the design document has the analysis. This is the plan for it, including the constraint you added: **MIDI Player must keep working, with no runtime cost.**

### The four changes

| | Change | Public surface |
| --- | --- | --- |
| C1 | `MidiSequencePlayer` honors `MidiSequenceTrackRouting.Connection`, `.Group`, `.ChannelOverride` | none — the properties and their documented behavior already ship |
| C2 | A sequence event can carry UMP words natively instead of MIDI 1.0 bytes | none — internal flag |
| C3 | `MidiSequenceBuilder` | contract 2 |
| C4 | `MidiSequenceTimingMode` — Musical or Absolute | contract 2 |

**C1 is a defect, not a feature.** The shipped IDL comment and the published `MidiSequenceTrackRouting` page both promise tracks can go to different endpoints simultaneously and stay together. They cannot today. That page should not stay wrong while we wait for the rest of the app.

### Protecting MIDI Player

The player compiles the native model directly rather than consuming the projection, so these are compile-time changes to it, not runtime ones. Specifically:

- **C2's branch is in `PlaybackEngine::Load`, per event, once** — the place that already converts the whole file. The playback sweep is untouched. Prepare time for a large file is the thing to watch, not playback.
- **C1 adds a destination index to `PreparedEvent` and one indexed lookup in `SendWordsUnderLock`.** `PreparedEvent` is 8 + 4 + 4 + 4 + 1 + 1 + 1 + 2 bytes; a `uint8_t` should land in existing padding. **Confirm `sizeof(PreparedEvent)` does not grow**, because `m_prepared` is one entry per event and the corpus has files at the two million event cap.
- **Measure, do not assume.** The Patchbay bench is the pattern: compile the real `midi_sequence_playback_engine.cpp`, hash-verified against the repo copy, and time `Load` on a large fixture, best of several. Report before and after.
- **The 65 TAEF sequencing tests must stay green throughout**, and C1 gets new ones that actually route two tracks to two endpoints and assert both received.

### Docs

`docs/sdk-reference/Utilities/Sequencing/` gains `MidiSequenceBuilder.md` and `MidiSequenceTimingModeEnum.md` (enum pages keep the `Enum` suffix in the URL, per the site conventions), the namespace index gains both, and `MidiSequencePlayer.md` and `MidiSequenceTrackRouting.md` get updated. Written from the IDL comments, same as the existing twenty.

### One thing to rule on first — ANSWERED

`midi_file_sequence.*` and `midi_sequence_playback_engine.*` are **not yet shipping**, and the rule is the **consumer rule**, not the location rule. **No servicing gate is needed for C1 or C2.** That removes the gate work, the gated tests and the `mididiag` listing entries from this phase entirely.

### Two more work items that belong with the API phase

**Share the endpoint match logic.** MIDI Patchbay's `EndpointMatch`, `EndpointMatchMode`, `LiveEndpoint` and `EndpointCatalog` (about 550 lines across `EndpointCatalog.{h,cpp}` and part of `PatchModel.h`) move into `midi-app-shared` as `midiapp::` types, so Patchbay and MIDI Glass use one copy. `Resolve` and `SuggestReplacement` change from taking a `PatchEndpoint` to taking an `EndpointMatch` plus its mode, which is a two-call-site change in Patchbay. Remember that **`midi-app-shared` is consumed file by file per project, not as a static library**, so both `.vcxproj` files get the entries. Patchbay must be rebuilt and re-driven afterwards — this is a refactor of working, shipped code, so it earns a regression pass rather than a build check.

> **Done, 23 September 2026.** Two things the estimate got wrong. It was **sixteen** call sites, not two, so rather than repeat the three-argument unpack sixteen times Patchbay gained `ResolveEndpoint` and `SuggestReplacementFor`, one place that knows a saved endpoint keeps its criteria, its mode and the name it last went by; the five `catalog` locals that existed only to call those went with it. And `Resolve` needed a **third** argument, not two: the name match falls back to the last known display name when the stored criteria carry no transport supplied name, which is behavior a match object alone cannot express. Shared code also cannot reach Patchbay's telemetry, so the four swallowed exceptions now go through `midiapp::SetEndpointErrorHandler`, which Patchbay wires to the sink it already logs to. Regression pass driven through UI Automation: endpoints enumerate, two added, connected, routed (a note on sent into one loopback arrived at the far end of the route), saved, reloaded after a restart, routed again, and with a device id deliberately broken in the saved file the offline bar appeared and the replacement was still offered by name.

**MIDI Glass appears to other apps as a virtual device**, not as a loopback pair. A virtual device's lifetime is tied to the session that creates it, which is exactly how this app already manages connections: the device shows up in a DAW while MIDI Glass is running and disappears when it is not, leaving no machine configuration behind. A loopback would persist in the service configuration after the app closed and would need cleaning up, which is a worse fit and more to go wrong. Optional per layout, off unless asked for. **Before testing this, confirm which midisrv is installed on the dev box** — the repository notes say the virtual MIDI lockup bug made virtual device tests unsafe there, and the fixed build needs to be confirmed present rather than assumed.

---

## Part D — the phases

Ordered so that risk is retired early, the SDK work lands while the app design is still settling, and nothing is built on a guess.

### Phase 0 — Spikes. Settle the two things a document cannot. **DONE 23 September 2026.**

**Where the work goes.** All spike code lives in `src/prototypes/midi-glass/spikes/`, versioned, following the `src/prototypes/midi-synth/` precedent. Nothing in the prototype folder ships, is wired into the installer, or joins a shipping solution. When the app itself is real it moves to `src/in-box/user-tools/midi-glass/` like every other tool.

**Spike 1: how a control is painted.** Three candidates, one measurement.

| Approach | Upside | Risk |
| --- | --- | --- |
| A templated XAML element per control | Automation, hit testing and theming are free | Visual tree weight at 200 controls; per-frame animation cost |
| One custom-drawn canvas | Fast, total control of the look | Every automation peer hand-written; code-built visuals do not re-theme themselves |
| Hybrid — a light XAML element per control for identity and hit test, custom-drawn content | Keeps automation, controls the paint | Two things to keep in step |

**The test:** a 200-control page, twelve of them animating, on the lowest hardware we care about. Measure frame time, input-to-send latency, and memory. **My hypothesis is the hybrid**, but the point of a spike is that the hypothesis can lose.

Include the theming trap Patchbay hit: anything built in code does not re-theme itself, and an app-level resource lookup ignores an element-level theme override. Whatever wins has to answer that.

**Spike 2: latency floor.** Pointer event to `SendSingleMessage`, measured, on touch and on pen. This sets the hot-path budget and tells us whether sending from the input handler is enough.

**Exit:** a rendering decision with numbers behind it, and a latency budget written down.

#### What it found

Full record in [MIDI-Glass-phase-0-findings.md](MIDI-Glass-phase-0-findings.md); the short version:

- **The hybrid wins, and it wins on the numbers.** Per frame it costs what pure composition costs (19 µs against 18 µs at the plan's own test size, level all the way to 1200 controls), because the XAML element is paid for once at build time and nothing it owns is invalidated by a value change.
- **A screen reader finds nothing on a composition-only surface** — zero elements, not a slider with a missing name. The hybrid and the templated approach both expose 200 named sliders whose value can be read and set through the range value pattern, proven by driving it.
- **Templated XAML holds 60 fps at the size the plan specified** and collapses to 9 fps only when a large page is entirely in motion. The specified test alone would have cleared it. Sweep the size.
- **Input to send is 264 µs**, of which our own code is 16 µs. Sending from the pointer handler is right; queuing to a render tick would make it sixty times worse. **`GetCurrentPoint` costs 15 µs, forty five times the MIDI send** — that is the expensive call on the hot path.
- **The theming trap did not bite any of the three**, because all three share brush objects rather than creating one per control.
- **Still unknown: labels.** The spike draws no text. The hybrid can put a `TextBlock` in the element; composition alone has no cheap answer without Win2D, which is not in this repository's package set. That became a third argument for the hybrid after the fact.
- **Not tested: low-end hardware, ARM64, touch, pen, multi-touch.** The full list is in the findings.


### Phase 1 — The SDK work. Independent, and it improves MIDI Player on its own.

C1 through C4, the docs, the tests, the before-and-after prepare-time measurement. Also the shared endpoint catalog extraction and its Patchbay regression pass.

**Exit:** all four landed, 65+ TAEF tests green, MIDI Player builds and plays with measured prepare time no worse than before, docs published, `MidiSequenceTrackRouting` no longer documents something that does not work. Patchbay builds, routes and saves exactly as it did before the catalog moved.

> **Done, 23 September 2026.** C1 through C4 landed, 83 TAEF tests green, docs published, prepare time 7946–8200 µs against 7856–7889 before. **Contracts in this namespace stay at version 1** — Pete's call, because the ABI is not locked yet, so a new type or member goes *into* contract 1 rather than being versioned. `MidiSequenceBuilder` is a new activatable class and needs an in-box manifest entry, which is outside this repository. The shared endpoint catalog moved to `midi-app-shared` and Patchbay was re-driven through UI Automation: endpoints enumerate, two added, connected, routed on the wire, saved, reloaded after a restart, routed again, and with a device id deliberately broken the offline bar appeared and the replacement was still offered by name.

### Phase 2 — The shell. Make it a member of the family.

Project scaffold copied from `midiscratchpad` per the family checklist, `WindowChrome`, `MidiAppSettings`, appearance flyout with the gear in the title bar, PREVIEW chiclet, single instance, icon, version resource, installer entry, `.resw` from the first string onward.

**Exit:** it launches, it looks like the family, it builds clean x64 and ARM64, it is in the installer, and the resw sweep is clean.

> **Done, 23 September 2026.** `src/in-box/user-tools/midi-glass/`, exe `midiglass`, title bar "Windows MIDI Glass". Builds clean x64 and ARM64 with zero warnings, verified through UI Automation, in the installer and the solution. The body is a deliberate "not finished yet" panel until phase 4 gives it a surface.

### Phase 3 — Document and themes. The data layer, with no surface yet.

`LayoutModel`, `LayoutStore`, `ThemeModel`, `ThemeStore`, the six built-in themes plus Pigment Light, Pigment Dark and Bigwig, contrast measurement, the version-and-round-trip rule, thumbnails, the page size templates and the size-derived control defaults.

**Exit:** a hand-authored `.midilayout.json` round-trips byte for byte, a thumbnail is generated without opening a window, and a file from a "newer version" keeps its unknown parts. Pure tests, no device needed.

> **Part done, 23 September 2026.** In `src/in-box/user-tools/midi-glass/document/` and `thumbnail/`, with 55 tests in `Midi2.MidiGlass.unittests` that need no window and no device. Landed: `LayoutModel` and its validation, the serializer, `LayoutStore`, the page templates and size-derived defaults, `ThemeModel` with all nine shipped themes and measured contrast, `ThemeStore` with themes as their own shareable files, and thumbnails. **Auto-save moved to phase 5**: there is no editor yet, so nothing can be dirty, and a debounced saver written now would be a timer with no input and no honest way to test it.
>
> **"Round-trips byte for byte" needed restating to be worth testing.** A file a person typed has their whitespace and their key order in it, so byte equality against the original can only be had by never reformatting, which gives up canonical output. What is enforced instead, and is the property that actually matters: **writing the same document twice produces identical bytes, and reading back what was written and writing it again produces identical bytes.** That is what keeps a layout diffable and stops a save from looking like an edit. It also forced a real decision — the writer is hand-rolled rather than `JsonObject::Stringify`, because a `JsonObject` is a map and will not promise to return keys in the order they went in. Reading still goes through the platform parser, because a layout is untrusted input and a hand-written parser is the wrong place to learn that.
>
> **Contrast measurement found three defects in the themes as designed**, which is the argument for measuring rather than eyeballing. Pigment Light's orange slot came out at 2.91 : 1 on its near-white deck. Magenta on black in High contrast is 6.70 : 1, under the bar that theme of all themes has to clear. And the design's claim that "the dark themes get away with hardcoding the track as black" is not true: Neon Booth's near-black deck against a black track measures 1.03 : 1, and High contrast measures exactly 1.00 : 1 — the track was invisible. Every dark theme now sets its own track color.
>
> **Thumbnails needed a second renderer, and Win2D is now in the package set.** The hybrid chosen in phase 0 is XAML plus composition, and neither will produce a frame without a window; a card has to exist for a layout never opened on this PC. `Microsoft.Graphics.Win2D` 1.3.2 draws onto an offscreen bitmap on a software device, which works with nothing on screen and no usable GPU. Generating one by hand, which is also how the headless path is tested: `midiglass --thumbnail <layout> <output png> [width]`, which returns before any window is created.
>
> **This does not re-open the phase 0 decision.** The hybrid won because a composition-only surface exposes **zero** elements to a screen reader, and that is unchanged. Win2D only removes the supporting argument about labels having no cheap answer. Win2D is for the offscreen card; the live surface still belongs to XAML and composition.
>
> **A theme file is the one thing here that arrives from a stranger and then gets drawn.** A layout is opened deliberately; a theme is the thing people swap on a forum. So a theme read from a file is never built in whatever it claims — otherwise a shared file could shadow a shipped theme in the picker and make itself unoverwritable — and the deck image is a bare file name inside the shared assets folder, never a path out of it. A half-written theme falls back to Studio Dark slot by slot, so somebody who has set two colors still has a legible surface rather than six black slots on a black deck.

### Phase 4 — The runtime surface. The part that has to feel right.

`SurfaceRenderer`, `InputRouter`, `BindingEngine`, `OutputRouter`, `DeviceCatalog`. Windowed runtime window. The three scale modes. Value throttling with its trailing send. Per-control initialization values and the global override. Feedback in. Panic. Device loss and return.

Test it the way Patchbay was tested: **write a layout file before launching**, so the probe never has to drive an editor that does not exist yet. That harness pattern is already proven here.

**Exit:** a hand-authored eight-fader layout sends correct messages on the wire, proven by capture, not by reasoning. Multi-touch moves two faders at once. Feedback moves a fader from a device. A throttled fader never ends on a stale value — assert the last message equals the final position. Pulling the device does not crash or hang it. Latency inside the phase-0 budget.

> **Started 23 September 2026 — the binding layer is in.** `src/in-box/user-tools/midi-glass/binding/`, 26 more tests, 81 in total, still no window and no device. `BindingEngine` resolves every device name to an index at load and then only does arithmetic into a caller-owned buffer; `ValueThrottle` takes time as an argument so the trailing send is testable without waiting.
>
> **The wire format was checked against an independent decoder, not against the arithmetic that produced it.** Hand-computed expectations and a hand-written builder can share the same misunderstanding and agree. So the words go out through a loopback and the MIDI console decodes them: `20B0077F` reads back as control change 7 value 127, `20E00040` as pitch bend center, `40903C00 FFFF0000` as a MIDI 2.0 note on at velocity 65535, `40B00700 80000000` as control change 7 at 2147483648. That is the "proven by capture" half of the exit criterion, for the message shapes.
>
> **Three decisions that are easy to get quietly wrong, so they have tests of their own.** 1.0 scales to the **top** of the range, not one short — a fader pushed all the way up sends 127, not 126, and the one-short version is the kind of thing nobody reports and everybody notices. A **program number is an identity, not a position**, so it is never scaled; scaling it would recall a different patch depending on where a fader happened to be. And a control's **destination protocol makes no difference to what leaves the app** — see below.
>
> **Corrected the same day, by Pete: the app does not fold to MIDI 1.0 at all.** My first version refused to send a registered controller to a MIDI 1.0 endpoint, on the grounds that expanding it into the four MIDI 1.0 messages was a different feature. That was wrong. **The service does the downscaling** — MIDI 2.0 protocol to MIDI 1.0 protocol, and UMP to MIDI 1.0 byte format — for whichever of the *client* or the *device* needs it. A registered controller sent as `40200509 12345678` arrives at a WinMM client as CC 101, CC 100, CC 6 and CC 38. So the app sends UMP at full resolution and lets the platform convert; the service owns the canonical conversion, every app on the machine gets the same one, and a second implementation here would only drift from it.
>
> **Values are a range, not an on/off pair, and either end can be a percentage or an exact number.** Device documentation does not talk in percentages: the APC40 Mk2 protocol says a clip LED is a note on where the note number picks the LED, the **channel** picks the display type and the **velocity** picks the color — channel 0 for a solid primary color, channel 9 to pulse at a quarter note, velocity 5 for `#FF0000`, 21 for `#00FF00`. A customer copying that table types 5 and must see 5 afterwards, not 3.9 %. So every message carries a `Minimum` and a `Maximum`, each independently `Fraction` or `Absolute`, and the control's position is interpolated between them. That one idea covers an ordinary fader (0 % to 100 %), a fader limited to MIDI 1.0 range (0 to 127 absolute, quantized onto whole numbers by the rounding), a button (off 0, on 127), a button on a MIDI 2.0 device (off 17, on 13005), a pad color, and an inverted fader — a minimum above a maximum falls out of the arithmetic with no extra switch. `UseMidi1Protocol` is separate again, because the need for exact values will arrive with 16-bit MIDI 2.0 velocities too. Verified on the wire: `20900005` is note 0 velocity 5 on channel 1, `20990015` is velocity 21 on channel 10, `40903C00 75300000` is a MIDI 2.0 note on at velocity 30000.
>
> **A continuous control can have stops, and the crowded case decides the design.** `Detents` on a message is `Continuous`, `EvenSteps` (a step in the same units as the ends — 0 to 100 % in tenths, or 27 to 127 in fives) or `ExplicitValues` (stops at 10, 17, 38, 39, 40, 57). **A listed stop gets an equal share of the travel** rather than sitting where its value falls between the ends. That is the whole design, not a detail: a version spaced by value was built and measured, and it reaches `10 17 57 57 57 57` — three of the six stops cannot be selected at all, because 38, 39 and 40 are a fortieth of the travel apart. `EvenSteps` is measured from the minimum so a range that does not start at zero still has a stop on its own bottom end. `DetentCount` and `DetentPosition` exist for the surface, which has to snap a finger and draw the notches; the engine produces the right value either way.
>
> Still open in phase 4: `DeviceCatalog`, `OutputRouter`, the runtime window, `SurfaceRenderer`, `InputRouter` and multi-touch, Panic, and the end-to-end capture. **The ordered list is at the top of this document**, under "What is left in phase 4".

> **Finished, 24 September 2026.** `runtime/` (`DeviceCatalog`, `OutputRouter`, `SurfaceScale`, `PanicMessages`), `surface/` (`GlassControl`, `SurfaceAutomationPeer`, `SurfaceRenderer`, `InputRouter`, `InputRules`, `SurfaceColors`), `RuntimeWindow.*`, and the library in `MainWindow`. 134 tests, still no window and no device. Both folders have a README stating their contract and, explicitly, what they do not do.
>
> **Proven on the wire, not by reasoning.** A layout written by hand before launching, run, and driven through UI Automation while the far end of a loopback was captured:
>
> | What was driven | What arrived |
> |---|---|
> | A fader to the top | `40B00700 FFFFFFFF` — control change 7 at full 32 bit scale, not one short |
> | A fader to the middle | `40B00800 80000000` — exactly half |
> | A fader limited to 0–127, MIDI 1.0 protocol, to the top | `20B00B7F` — 127 |
> | The same fader to the middle | `20B00C40` — 64 |
> | A pad | `2090247F` then `20802400` — note on, note off |
> | A control marked to send its value on load | `20B01440` once, and only once |
> | Panic | 64 messages: sustain off, all notes off, all sound off and pitch bend center on all sixteen channels of the **one group the layout drives**, and nothing on the other fifteen groups |
>
> Feedback in was proven the same way, sending into the far end: `40B01E00 FFFFFFFF` moved a fader to 1.0, `40B02000 80000000` moved another to 0.5, and a MIDI 1.0 `20B01F40` moved a third to 64/127. Controls the messages did not address stayed where they were.
>
> **Two layouts ran at once in one process and shared one connection**, proven by both appearing on the same capture. Closing the library window left both running, which is the behavior somebody mid set needs.
>
> **The probe found two real defects, and that is the argument for driving the thing rather than reading it.** Setting a value through automation sent nothing at all, because the code treated it as the end of a gesture and a gesture that never started has nothing held back to release; a discrete set is now a whole gesture — touched, moved, released. And the startup values were replayed every time the device watcher fired, which is once per endpoint on the machine at launch and again whenever anything is plugged in: a layout would have pushed a whole desk back to its opening positions because somebody connected a webcam. Connections are now only rebuilt when what this layout resolved to actually changed.
>
> **A review of the diff found a third, which never reached the wire.** The shared endpoint catalog takes exactly one changed handler, so a second runtime window silently replaced the first window's, and a closed window never removed its own — a device arriving after that would have called into freed memory. One process-wide subscriber now fans out to every open catalog, and closing one removes it.
>
> **The theme model could not describe one of its own shipped themes.** Bigwig's plate is a neutral raised grey and every other theme's is derived from the glass tint or the fill at rest, so there was nothing to put it in. `Theme` gained a `PlateColor` whose alpha of zero means "work it out", which is what the other eight do.
>
> **A per control send interval was added to the document**, because the throttle the plan called for had nothing to read. Zero is no limit, which is what buttons, notes, sequences and system exclusive use.
>
> **Not verified**: touch, pen and multi-touch (no touch hardware on this machine — the per-pointer capture that makes it work is XAML's, and the code takes one pointer per control, but it has not been driven with two fingers); pulling a device out mid performance and putting it back; ARM64 at run time, though it builds clean; low end hardware; a screen reader end to end, though every control is exposed as a named slider, button or check box and both the range value and invoke patterns were driven through automation.

### Phase 5 — The editor.

Palette, drag and click placement, snap, magnetic guides, spacing, Repeat, the inspector, undo and redo, Edit/Try, the monitor rail, the Outline pane, keyboard order editing, **the virtual canvas with its grow and shrink paths and off-page handling**, and **auto-save with the Patchbay chip** — "Saved" / "Not saved", written about a second and a half after the last edit, moved here from phase 3 because it needs an editor to have something to save.

**Exit:** a layout built entirely through the UI, then run, then edited again. Undo survives a drag, a repeat and a delete. A page grown then shrunk back leaves every control exactly where it started. Everything reachable by keyboard.

> **Done, 24 September 2026.** `editor/` (`EditGeometry`, `ArrangeOps`, `RepeatPlan`, `UndoStack`, `ControlFactory`, `EditorController`), `EditorWindow.*` and its five view files, and `binding/MonitorFormat`. 243 tests, still no window and no device. The editing engine is free of XAML on purpose, so every rule below is checked without a window.
>
> **Driven end to end through UI Automation, 37 checks.** The outline fills the inspector; a typed position auto-saves and reaches the file; undo and redo walk the stack; the palette adds a control; Repeat builds a bank of eight; the page grows to 1920 x 1080 and one undo puts it back with **every control exactly where it started**; Try mode sends; the monitor catches it; Clear empties it; Edit comes back and a control driven there sends nothing.
>
> **Try mode is where the shared layer came from.** Edit and Try have to send exactly the same thing, and two copies of the device table, the connections, the engine and the throttles would drift apart. `runtime/LivePlayer` now holds all of it and both the runtime window and the editor drive one. The runtime window's phase 4 capture was re-run afterwards and still produces the same seven messages, which is what makes the extraction safe to believe.
>
> **The monitor rail reads the message back rather than reporting what was asked for.** Printing the request would agree with the arithmetic that built it even when the arithmetic is wrong, which is exactly the question somebody opens the monitor to answer. `DescribeMessage` decodes the words that actually reached a connection; on the wire and in the rail, a fader at the top reads `40 B0 07 00  FF FF FF FF` and **`CC 7 = 1.000`**, not 0.999.
>
> **Three defects the probe found, all of them in the app rather than in the test.**
> 1. **Redo was disabled immediately after an undo**, so the button existed but could not be used until something else refreshed the toolbar.
> 2. **A `ToggleButton`'s `Click` never fires for assistive technology.** UI Automation's Toggle calls `OnToggle`, which raises `Checked`/`Unchecked` only — so Edit and Try moved the highlight and changed nothing else for a screen reader user. Both toggles and both monitor switches now use `Checked`/`Unchecked`.
> 3. **Moving to `Checked` crashed the window on open**, because `Checked` fires during `InitializeComponent` while the `x:Name` fields are still null, and calling a method on a null C++/WinRT projected type is an access violation rather than an exception a `catch` can hold. Everything those handlers touch is gated on a loaded flag set at the top of `Loaded`.
>
> **The editor says what the devices are doing in Try mode**, because a monitor with nothing in it cannot tell the difference between nothing sent and nowhere to send it. The status bar reads "Trying · N device(s) connected", names a device that is missing, and distinguishes a device that is present from one that would not open.
>
> **Not verified**: touch and pen on the canvas, a screen reader end to end, and ARM64 at run time. Drag, rubber band and handle resizing are covered by the engine tests but have not been driven with a real pointer, because a probe that synthesizes a mouse takes the machine away from whoever is using it.

### Phase 6 — Sequences, generators and learn.

`SequenceRunner` on the phase-1 API, `GeneratorEngine` on `BeatClockGenerator` with one process-wide tick, the tempo source, and all three Learn modes including learn-while-running.

**Exit:** a button plays a six-step sequence with a wait in the middle, proven on the wire with timing. Twenty simultaneous sequences do not cost twenty threads. Learn fills endpoint, group, channel and number from one wiggle, and a bank learn fills eight controls in order.

### Phase 7 — Full screen, multiple windows, displays.

Borderless full screen, the fading Esc toast, **the single corner button and its flyout, with the corner chosen per layout**, display-required while running, per-layout display memory, several runtime windows in one process, View mode with zoom and pan, **the optional virtual device**.

**Exit:** two layouts running on two displays at once, sharing one connection to a shared synth. Full screen switches without reconnecting anything. Unplugging the second display does not strand a window. A DAW sees the virtual device appear and disappear with the layout, and nothing is left in the machine configuration afterwards.

### Phase 8 — Accessibility and the safety pass.

Automation peers, reduced motion, minimum touch size, the accessibility checker page, the imported-layout SysEx prompt, and a security review of the file reader against a malformed or hostile layout.

**Exit:** driven end to end with a screen reader. `check_accessibility.ps1` clean. A fuzzed layout file does not crash it.

### Phase 9 — Docs, templates, polish.

The `docs/tools/midiglass/` page following the site conventions, the starter templates, first-run, and whatever the previous eight phases turned up.

---

## Part E — how this gets verified

Same discipline as the rest of this repository, stated so it is not optional:

- **Drive the real UI.** UI Automation plus screenshots, reading values back out. Count-only assertions hide wrong-control bugs.
- **Ask before taking the cursor.** Anything that synthesizes real mouse or touch input takes the machine away from whoever is using it. Unless the session is running unattended, ask first. UI Automation and reading values back do not need this; `SetCursorPos`, `mouse_event` and synthesized touch do.
- **Prove MIDI on the wire**, not in the debugger. The capture harness exists and has caught real defects.
- **Write the layout file before launching** for anything that would otherwise need the editor.
- **Never leave a window maximized**, and put back any setting a probe writes.
- **Report what was not verified.** Every phase exit above should come with that list.

---

## Part F — sequence at a glance

```
Phase 0  Spikes                  rendering decision + latency budget
Phase 1  SDK                     C1..C4, docs, tests        <- independent, start alongside 0
Phase 2  Shell                   family chrome, installer
Phase 3  Document and themes     model, store, 9 themes     <- needs 2
Phase 4  Runtime surface         render, input, bind, route <- needs 0 and 3
Phase 5  Editor                                             <- needs 4
Phase 6  Sequences, generators, learn                       <- needs 1 and 4
Phase 7  Full screen, windows, displays                     <- needs 4
Phase 8  Accessibility and safety                           <- needs 5 and 7
Phase 9  Docs, templates, polish
```

Phases 1 and 2 can run at the same time; they touch nothing in common. Phases 6 and 7 can run at the same time once 4 is done.

---

## Part G — what is settled, and what is left

**Settled.** The name is **MIDI Glass**, and Pigment Light, Pigment Dark and Bigwig keep theirs. Fixed page size from a template with letterboxed scaling, and the virtual canvas with off-page controls. Actual size as the run-time default with fit as one press. No scripting engine. A virtual device rather than a loopback. A per-layout device table with the match logic shared out of Patchbay. Touch sizes chosen by the customer with defaults derived from the page. Default values only, with per-control initialization and a global override. One process with several windows. Bank learn is in scope; learn-while-running is not. Drop shadows deferred. No servicing gate on the SDK work.

**Left to decide, and none of it blocks starting.**

1. **How far does the virtual device go?** One endpoint for the whole app, or one per running layout? One per layout is cleaner in a DAW's device list and matches "a layout is the unit", but it means a device appearing and disappearing as layouts open and close, which some DAWs handle badly. I lean toward **one endpoint per running layout, named after the layout**, and would want to try it against a real DAW before committing.
2. **Does the Outline pane live beside the canvas or replace it?** Beside it costs width on a laptop; a toggle costs a keystroke. Probably a toggle that remembers, but worth seeing at phase 5 rather than deciding now.
3. **What is in the layout settings flyout versus app settings?** Roughly: anything that travels with the file is layout, anything about this PC is app. The edges — tempo source, virtual device on or off, initialization override — are arguable and will be obvious once there is something to hold.
