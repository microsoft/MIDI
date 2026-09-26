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

- Shipped themes: **Studio Dark**, **Neon Booth**, **Daylight**, **Blueprint**, **High contrast**, and the seven worked out below — Pigment Light, Pigment Dark, Bigwig, Bone, Cathode, **Amber Console** and Terminal Green. Amber Console was a swatch in this picker from the start; screen 17 is that swatch finally built, which is why it keeps the name rather than getting a new one.
- Deck: one color, a two-stop gradient, or an image.
- Control defaults in the theme: corner rounding, glass tint percentage, glow strength, label placement. This is how a customer makes their whole surface squarer or flatter in one move.
- **Contrast is measured, not guessed.** Each slot is checked against the deck and flagged before it ships to a stage. In the mockup, slot 5 at 4.1 : 1 is called out with what to do about it.
- **Each theme carries one sentence about what it costs**, shown in the picker. Not every theme can be equally accessible, and that is better said than quietly designed around. See the end of this section.
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

*Superseded for the shadow part only:* Bone, below, is built on an elevation shadow, and the surface renderer now draws one under every plate anyway. See that section for what actually changed.

### Bone

**Screen 15** (`15-bone.html`). A warm light theme made of two colors: **#E3DAC9** for the space and **#8A795D** for everything that has to read as dark. The deck is bone, or a lifted bone at the top of the gradient, and nothing else.

**The reason it is a different kind of theme.** Every other theme tells a control from its deck by *value* — a dark plate on a darker deck, or the reverse. Bone cannot. A warm white plate measures **1.23 : 1** against a bone deck, which is nothing at all. So the separation is carried entirely by a soft warm **shadow** under the plate, and the thing that lights up is **white** rather than the control's own color. The other themes make a control by darkening it; Bone makes one by lifting it.

That single change gives the surface a physical grammar the dark themes do not have, and it is worth naming because a customer will feel it before they can describe it: **what your hand touches is raised, what holds a value is carved, and what is dead is flush.** A fader cap stands off the plate while its slot is cut into it. An XY field is sunk. A disabled control loses its shadow and sinks back into the deck, which says "this does nothing" more plainly than any amount of fading can on a light surface.

**Four things it needs that the model does not have.** A **shadow color**, because a black shadow on a bone deck goes a dirty gray and this theme lives or dies on that shadow being warm. A **shadow spread**, because elevation only ever said how *dark* a shadow was, never how far it reached — the renderer hardcoded a 3 pixel blur, and measured on screen that darkens the single row of pixels under a plate by four values and then stops, which is not a shadow. A **light source** for the bloom — the control's own color as today, or white. And a **resting rim strength**, because a hairline that reads as a line on near-black is not there at all on near-white.

The first two were predicted as one; they are two because they answer different questions and a theme editor has to show both. The fourth was not predicted at all and came out of looking at the thing on screen at four times size.

**The elevation shadow itself is no longer a cost.** The surface renderer already draws a masked shadow under every plate and the dark themes simply keep it low, so Bone turns it up and tints it rather than adding a layer. That closes the deferral in the table below, and it closes it on measurement rather than on preference. Every theme shipped before Bone keeps a spread of 3 and a black shadow, which reproduces the two constants they were drawn with exactly.

**Four things the numbers decided, not taste.** Ochre at readout size measures 3.82 : 1 on bone and a number needs 4.5, so readouts are ink rather than color. Shadow cannot carry warm white at 4.5 either, so the filled state deepens 16 % and lands at 5.5 : 1. Plate against deck is 1.23 : 1, which is why the shadow is structural and why a theme editor should refuse to let somebody drag elevation to zero on this theme — they would not get a flatter surface, they would get a blank sheet. White against bone is 1.39 : 1, which is why the rim always comes up to full color at the same moment the white light appears: anyone who cannot see the glow still sees the outline change.

**A hairline rim has to be nearly full strength here.** On a near-white plate the resting rim at the opacity the other themes use simply vanishes, and the control's color identity goes with it — six faders on six different slots all look the same. Bone runs the rim at 85 %. Measured against each theme's own plate, the 28 every other theme uses lands at 1.8 : 1 to 2.2 : 1 on the six dark themes and only 1.4 : 1 on the light ones. The "nothing is saturated at rest" rule survives because the *area* of color is still one pixel, and because all six slots are muted earth tones rather than neon.

The honest trade: in a dark room this is a lamp pointed at the performer. Bone is for a desk by a window, a classroom, a studio with the lights on — somewhere a near-black surface is the thing that looks wrong. Studio Dark still wins on a stage, and the theme picker should say so rather than letting somebody find out during a set.

Bone and Shadow are the two given colors. The other four slots — sage `#5C6E4E`, ochre `#8F6318`, clay `#9A543E`, slate `#5A647C` and brick `#9B3D34` — are chosen to sit with them, and every one measures 3 : 1 or better against both the plate and the deck.

### The retro set — Cathode, Amber Console and Terminal Green

**Screens 16, 17 and 18.** Three old screens, one ramp machine. Each is the tonal machinery with the hue wash off, the way Bigwig and Bone are, and together they are what finished the theme model — by the third one, nothing new had to be added.

**Cathode** is a black and white television, and it is neither. The glass is a sour olive green when nothing is driving it, the phosphor is blue-white, every edge is soft because a beam has no hard edge, and there is no pure black or pure white anywhere — the palette runs `#101611` to `#F4F8FF` and a sweep of the rendered deck never leaves it except where a scan line crosses a carved slot. What makes it unlike every other theme is that **a tube has one phosphor, so there is no color to tell one control from another with**. Brightness has to carry identity, value and state at once and it cannot: the widest pair of its six slots is 2.32 : 1. So a control here is known by where it sits and what it is called, which is also the first time the full-screen rule about Panic keeping its place has had to do all the work on its own.

**Amber Console** is the P3 terminal, and it is the swatch that had been sitting in the picker since the beginning. Three things make it: the glass is a warm maroon, **the glow is redder than the thing casting it** because P3 decays through red, and brightness moves hue — the ramp runs like heat, from deep ember through orange and amber and yellow to white. That last part gives it something Cathode has not got at all, a second channel, and the numbers say so. What it costs is stated rather than engineered away: the deepest ember fails as type, so the bottom rung of the ramp carries light and never letters.

**Terminal Green** is the green screen, and **the glass is not green**. A terminal has a tinted anti-glare faceplate, so the unlit screen is a cool blue-slate and the phosphor sits a long way from it in hue — built from memory this comes out as green on dark green, which is Cathode with the colors swapped and makes two of the three look like one idea. Every photograph of real hardware also has the room reflected across the upper left of the glass, which is what makes a faceplate read as glass rather than as paint. It has the highest rim contrast of the three and it is the only one where every slot still works filled with a label on it.

All three share the same grammar: a dim raster box that separates from the glass by its own spill rather than by value, inverse video for a latched control, snow instead of a red light when a device has gone, and a value that is hottest at the leading edge and decays behind it. And all three keep the original rule — a control never uses more than one color, and that color only ever appears on the rim, the value and the fill.

### The theme model, consolidated

Seven themes past the original six have each asked for one small thing, and together they settle what a theme actually is. This is the list to build against.

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
| **Elevation shadow** | strength, and a **color** so it can be warm rather than black | Bone |
| **Shadow spread** | how far the shadow reaches, in pixels. Elevation is how dark it is | Bone |
| **Resting rim strength** | 0 to 100. A hairline needs far more of it on a light plate | Bone |
| **Bloom color** | a color, or derived from the control's hue. **Replaces Bone's light source** | Cathode, Amber, Green |
| **Resting glow** | 0 to 100, separate from the glow that activity causes | Cathode |
| **Scan lines** | pitch in page pixels, strength, color | Cathode |
| **Deck falloff** | how far the corners drop below the deck's own floor | Cathode |
| **Plate sheen color** | so a warm plate lifts warm instead of going grey | Amber |
| **Arc track color** | the knob arc's track, which is on the deck rather than on the plate | Cathode |
| **Meter zones** | which three slots the meter's three levels use | Cathode, Amber, Green |
| **Faceplate sheen** | strength and color of the room reflected in the glass | Terminal Green |
| **Caution** | one sentence the theme picker shows about what this theme costs | Amber |

The track color is worth calling out separately: it is not a cost of any of these themes, it is a gap in the original model. The dark themes get away with hardcoding it as black, and the first customer who built a light theme of their own would have hit it.

The elevation shadow was deferred once, for a good reason — a composition shadow under every control is a real cost at eighty of them. It comes back here because the renderer already draws one and the dark themes just keep it low, so Bone is turning an existing layer up rather than adding one. What is genuinely new is the **color**, and that part is not optional: a black shadow on a bone deck goes a dirty gray.

**Bone's light source became a color.** It was an either-or — the control's hue, or white — and that was right for Bone, where the only room left to say "this just did something" is to take a near-white plate the rest of the way. It is not enough for a phosphor. Cathode's light is blue-white because a monochrome tube is not white. Amber's halo is **neither the hue nor white**: it is ember, redder than the thing casting it, because P3 decays through red, and that single fact is what people recognize as an amber screen. Terminal Green's is yellow-green, the same shift at the other end of the spectrum. A bloom derived from the hue cannot express any of those, so the property becomes a color with "derive it" as its default, and every theme that shipped before renders exactly as it did.

**That is the third time the same thing has happened, and it is worth naming.** Anything the renderer works out from pure white or pure black eventually meets a theme that is neither. The shadow was the first (Bone). The bloom is the second. The plate sheen is the third: it is a percentage of white today, and on a warm plate that desaturates rather than lifts — measured on Amber, white at 7 % over the plate overshoots blue by eight counts and the warm lift goes grey. **If a fourth percentage-of-white is ever added to this model, give it a color when it is born.**

**Scan lines are the one genuinely new thing to draw, and they are cheap.** One overlay for the whole deck, never anything per control, so eighty controls cost what four cost — the corner falloff and the faceplate reflection ride along in the same overlay. The one thing that has to be right: **they are drawn in screen pixels after the page has been scaled.** A three pixel pitch in page units at 87 % zoom is a beat pattern across the whole screen. Cathode asked for them first; Amber and Terminal Green wanting them too is what turns them from a one-off into a property.

**A slot and an arc are not the same kind of empty.** A fader slot is a recess cut into the plate, so its track has to be darker than the plate. A knob arc sits outside the plate, on bare deck, and on a dark theme there is nothing left out there to be darker than — its track has to be a faint light instead. Getting that wrong is not subtle once it is on screen: the knob shows where it is without ever showing how far it can go. One track color cannot answer both questions, so the arc gets its own with "same as the track" as its default.

**Every recess comes from one color, and nothing in the renderer picks a dark by eye.** Amber's palette was clean and its composite was not: hand-chosen recess colors with a scan line and the corner falloff laid over them drove the blue channel to zero, which is a pure black inside the one theme whose whole claim is that it does not have one. The fix was a single floor color used at different opacities everywhere. The check that catches it is a sweep of the rendered deck for any channel at 0 or 255, and it belongs in the theme tests.

**A theme must not choose a typeface.** All three retro themes are pictured in monospace and all three starter layouts use one, but the font belongs to the layout. A theme that reflowed somebody's labels because they liked the colors would break a page that was laid out around those labels.

### Measuring a theme: contrast is not the only question

Contrast answers "can this be read". It does not answer "can these two controls be told apart", because it counts brightness only and scores a change of hue at nothing. For that, measure the **color difference** between adjacent hue slots — CIE Δ*E*, where about 2.3 is the smallest difference anyone notices. The retro set is what proved the point: three themes, the same six slots and the same machinery, and wildly different answers.

| Theme | Mean Δ*E* between adjacent slots | Smallest pair | Widest pair | Dimmest slot used as type |
| --- | --- | --- | --- | --- |
| Cathode | 6.5 | 5.3 | 2.32 : 1 | 4.53 : 1, the only one that passes |
| Terminal Green | 20.4 | 8.7 | 3.60 : 1 | 3.25 : 1, fails |
| Amber Console | 21.9 | 13.6 | 4.18 : 1 | 3.02 : 1, fails |

No pair of Cathode's six slots reaches 3 : 1, so **nothing on that theme can be grouped by color** and the picker has to say so. Amber and Terminal Green both can. And each of the three fails somewhere different — Cathode is the only one whose dimmest slot can carry type, because it has no dim end; Terminal Green is the only one where every slot still works filled with a dark label on it; Amber has the widest pair. None of them wins outright, so the picker should say what each one is for rather than ranking them.

**Brightness is not the same question as loudness either.** Terminal Green looked far too bright next to the other two until it was measured: the three comps draw identical controls in identical places, and a sweep of the deck put mean luminance at 6.2 % for Cathode, 4.6 % for Amber and 4.7 % for Terminal Green. Cathode is the brightest of the three. Green only reads as the loudest because it is more saturated, and nothing in a contrast check counts saturation. Worth knowing before somebody turns a theme down to fix a problem that was never brightness.

### Not every theme can be equally accessible

We do what we can, and some themes will still need ordinary color vision. That is better said out loud than quietly designed around, because the alternative is washing a palette out until it clears every threshold and loses the thing that made it worth having. Amber's deepest ember fails as type at 2.65 : 1 and it stays, because it is the color that makes the theme look like a real amber tube. What replaced the fudge is a rule the editor can enforce: **the bottom rung of the ramp is light, not letters** — it carries a rim, a value or a fill, and never type.

Three things keep that honest. Each theme carries **one sentence in the picker** about what it costs. Every ramp is a rising **brightness** as well as a rising hue, so with no color vision at all a theme degrades to a plain brightness ladder and no further — Amber runs 18 / 28 / 40 / 53 / 71 / 90 %, Terminal Green 22 / 34 / 45 / 56 / 75 / 93 %. And **High contrast is always one press away**, offered when a layout is opened and never in the middle of a set.

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

**MIDI Glass.** Executable `midiglass`, title bar **Windows MIDI Glass**, Start menu **MIDI Glass**. The theme names **Pigment Light**, **Pigment Dark**, **Bigwig** and **Bone** stay too.

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
| `15-bone.html` | Bone — a warm light theme where the shadow, not the value, separates a control from the deck |
| `mock.css` | Shared styles for all fifteen |
| `serve.ps1` | Local static server on port 8742 |
| `shots\` | PNG captures of all fifteen screens |
| `MIDI-Glass-implementation-plan.md` | Remaining design gaps, the engine layering, the API work, and the phases |

These live in `src/prototypes/midi-glass/design/` so the design record is versioned. Nothing in this folder ships; when the app is real it goes to `src/in-box/user-tools/midi-glass/` like every other tool.
