---
layout: kb
title: How MIDI Glass themes work
audience: everyone
description: What a Windows MIDI Glass theme is, where theme files go, what every setting in a theme file changes on screen, and how to design a theme for someone else.
categories:
  - Developer Guidance
---

Windows MIDI Glass ("MIDI Glass") is the Windows MIDI Services app for building your own touch control surface. You put knobs, faders, pads, and buttons on a page, tell each one what MIDI to send, and play it with a finger, a pen, or a mouse.

Two things decide what you see. A **layout** says what's on the page: where each control sits, how big it is, what it's called, and what it sends. A **theme** says how the page looks. Put a different theme on a layout and every control keeps its place, its name, and its messages. Only the look changes.

This article is about themes: the theme file, where it goes, and how MIDI Glass turns each setting into what's on the screen. It doesn't cover building layouts.

> **MIDI Glass is a preview app.** The theme file described here is version 1. Later versions can add settings, and a theme file written for an older version keeps loading when they do.

> **For AI agents:** This article is written so you can design a theme for someone without reading the MIDI Glass source code. Read it once from start to finish. Then use [Designing a theme for someone else](#designing-a-theme-for-someone-else) as your process and [Every key in a theme file](#every-key-in-a-theme-file) as your reference. Notes marked **For agents** point out mistakes that are easy to make and hard to see, because you usually can't look at the result yourself.

On this page:

- [What a theme decides](#what-a-theme-decides)
- [The look MIDI Glass is built on](#the-look-midi-glass-is-built-on)
- [Colors: six slots and a neutral](#colors-six-slots-and-a-neutral)
- [Where theme files live](#where-theme-files-live)
- [Inside a theme file](#inside-a-theme-file)
- [How each setting changes the page](#how-each-setting-changes-the-page)
- [How a theme draws each control](#how-a-theme-draws-each-control)
- [The shipping themes](#the-shipping-themes)
- [Designing a theme for someone else](#designing-a-theme-for-someone-else)
- [Every key in a theme file](#every-key-in-a-theme-file)

## What a theme decides

| A theme decides | A layout decides |
| --- | --- |
| Six colors, and one neutral that means "no color" | Which of those colors each control uses |
| The deck: the page's background, its texture, and anything laid over it | Where each control is, and how big it is |
| How each kind of control is drawn: its body, outline, light, shadow, cap, and lamp | Each label's text, font, and size |
| Where labels go, unless a control says otherwise | Any single control that should look different from the theme |
| How controls light up when you touch them or when MIDI arrives | What each control sends, and what it listens for |

**A theme never picks a font.** A theme that changed fonts would reflow the labels on a page that somebody laid out carefully around them. Fonts belong to the layout.

## The look MIDI Glass is built on

The default look, and the starting point for most themes, is four ideas:

- **The plate** is the body of a control. On most themes it's smoked glass, a little see-through, sitting on the deck.
- **The rim** is a thin outline in the control's color. At rest it's faint.
- **The value** is light in the control's color that shows where it's set: an arc around a knob, a bar up a fader's slot, a dot on an XY pad.
- **The glow** is what happens when you touch a control or MIDI arrives for it. It lights up and fades back.

One rule holds these together: **a control uses one color, and that color shows only in its rim, its value, and its glow.** Nothing is bright when nothing is happening. That's what keeps a page of a hundred controls readable, and it's why switching themes is a color swap rather than a redesign.

The light themes add a second idea: **what you press is raised, what shows a value is sunk into the surface, and what does nothing is flat.** A fader cap stands up off its slot, an XY pad's field is cut in, and a control whose device is missing is dimmed.

Several shipping themes bend these rules on purpose. Jove's buttons are solid color at rest, and Airy System keeps every knob lit. Each theme that bends a rule says what it costs in the theme gallery, so you can decide before you take it on stage.

![The same page of controls in all sixteen shipping themes]({{ site.baseurl }}/assets/images/midiglass-themes-gallery.jpg)

## Colors: six slots and a neutral

A theme has six colors, called **hue slots**. A control doesn't store a color. It stores a slot. Change the theme and every control on slot 3 changes together, so a row of mute buttons still matches, and still stands apart from the row of solo buttons next to it.

- MIDI Glass numbers the slots 1 to 6. In a theme file they're a list of six colors, and the first one is slot 1.
- New controls start on slot 1, so make it a color you'd be happy to see on most of the page.
- The **neutral** is a seventh choice, and it means "no color." It's for controls that aren't part of any color group, like a row of plain utility buttons. A control on the neutral slot stays uncolored when the theme changes. On a theme with no neutral, those controls use slot 1.
- A control can also use a literal color, like `#FF0000`. A literal color ignores the theme, so it's only for the rare control that has to be one exact color.

If you're designing a theme for layouts that already exist, keep each slot's job. On Studio Dark, slot 1 is blue, 2 is green, 3 is amber, 4 is orange red, 5 is purple, and 6 is cyan. A customer who put all their drums on slot 4 will expect slot 4 to stay the warm one.

> **For agents:** Ask the customer what each color means on their layouts before you change the order of the slots. Reordering slots recolors every layout they have.

## Where theme files live

### The built-in themes

Sixteen themes come with MIDI Glass. They're part of the app rather than files on disk, so nothing can delete or change them. When you edit one in the app, you're editing a copy.

### Your own theme files

Your own themes are files in this folder, one theme per file, each ending in `.miditheme.json`:

```
Documents\MIDI Layouts\Themes
```

- MIDI Glass creates the folder the first time it needs it. You can also create it yourself.
- **The Documents folder isn't always `C:\Users\<name>\Documents`.** On many PCs it has been moved into OneDrive. Open File Explorer, select **Documents**, and look for **MIDI Layouts** there.
- Every theme in the folder shows up in the gallery for every layout on that PC, after the sixteen built-in ones.
- The file name doesn't have to match the theme's name. When MIDI Glass saves a theme, it names the file after the theme and swaps any character Windows doesn't allow in a file name (`\ / : * ? " < > |`) for an underscore.
- MIDI Glass reads the folder each time it shows the gallery. If you add a file while the app is open, open the layout's **Appearance** settings again to see it.

A theme file shows up in the gallery only if all of these are true:

- It's valid JSON, saved as UTF-8 **without** a byte order mark.
- It has a `name`.
- That name isn't a built-in theme's name. Three older names count too: **Pigment Light**, **Pigment Dark**, and **Amber Console**.
- No other file in the folder with the same `name` sorts ahead of it. When two files share a name, the one whose file name comes first alphabetically wins.

MIDI Glass doesn't show an error for a file it skips. The theme just isn't there.

### A theme inside a layout

A layout file names its theme in its `theme` value. It can also carry a whole theme of its own in a `themeColors` block, which holds the same settings as a theme file:

```json
{
  "theme": "Supersaw",
  "themeColors": {
    "name": "Supersaw",
    "hueSlots": [ "#E8822A", "#FF6047", "#4CE57A", "#96B7E2", "#E2CE7E", "#B09CE2" ],
    "fillWhenOnPercent": 0
  }
}
```

When you change a theme's settings in the MIDI Glass editor, the edited theme is stored in the layout this way. That's why a layout you send to a friend still looks the way you built it, even if they've never seen your theme.

> **For agents:** A `themeColors` block isn't a list of changes to the theme the layout names. It's a whole theme, and anything it leaves out comes from Studio Dark, exactly as in a theme file. The short example above would draw Studio Dark's look in Supersaw's colors, not Supersaw. The app always writes every setting into this block, and so should you.

### Which theme a layout uses

1. If the layout carries its own theme in `themeColors`, it uses that.
2. Otherwise it uses the theme the layout names, looking in the built-in themes and then in your Themes folder. An old name, like Pigment Light, finds the renamed theme.
3. If that name isn't found, it uses Studio Dark.

Picking a theme in the gallery drops any copy the layout was carrying and makes the layout point at that theme by name again. So when you improve a theme file later, every layout that names it picks up the change the next time it opens.

### Sharing and installing a theme

- **To share a theme,** send its `.miditheme.json` file.
- **To install a theme you were sent,** copy the file into `Documents\MIDI Layouts\Themes`, or open a layout in the editor and use **Import a theme…** on its **Appearance** page. Import copies the file into the Themes folder under the theme's name, replacing an older file with that name, and puts the theme on the layout you have open.
- **To save a theme from the app,** use **Save these colors as a theme…** on the **Appearance** page. MIDI Glass won't save under a built-in theme's name. After saving, the layout points at the new file by name instead of carrying its own copy.
- **To share a layout that uses a theme of your own,** make sure the layout carries the theme. Exporting a layout package doesn't include files from your Themes folder, so if the layout only names your theme, the person you send it to sees Studio Dark. Send the theme file along with it, or change any setting on the layout's **Appearance** page so the layout stores its own copy.

## Inside a theme file

A theme file is one JSON object. This one is small, but it's complete enough to use:

```json
{
  "fileVersion": 1,
  "name": "Harbor",
  "hueSlots": [ "#5CC8FF", "#7BD88F", "#FFC857", "#FF8A65", "#C792EA", "#FF6B84" ],
  "neutralColor": "#D8DEE9",
  "deck": {
    "kind": "gradient",
    "color": "#1E2B33",
    "gradientEndColor": "#0C151A"
  },
  "glassColor": "#22323B",
  "trackColor": "#0D1418",
  "arcTrackColor": "#1FFFFFFF",
  "meterSlots": [ 1, 2, 5 ]
}
```

Everything this file leaves out comes from Studio Dark, so Harbor gets Studio Dark's glass plates, faint rims, and glow, with its own colors on a blue slate deck. Measured, every slot is at least 5.3 : 1 against the top of the deck and 4.9 : 1 against the plate, and the two closest slots, the coral and the red, are still clearly different colors.

The app reads a theme file by these rules:

- **Strict JSON.** No comments, and no comma after the last item in a list or object.
- **UTF-8 without a byte order mark.** A file that starts with a byte order mark doesn't load. In Notepad, **UTF-8** is right and **UTF-8 with BOM** isn't.
- **Colors are text:** `"#RRGGBB"`, or `"#AARRGGBB"` with the **alpha first**. `"#80FF0000"` is red at half strength. That's the opposite of CSS, where the same color is `#FF000080`. Upper and lower case letters both work.
- **Alpha `00` means "not set."** Many colors are optional. Leave them out, or write `"#00000000"`, and the app works out a color itself. The [key reference](#every-key-in-a-theme-file) says what each one falls back to.
- **Numbers are JSON numbers,** not text. Most are whole numbers from 0 to 100 that act as percentages. Four are fractions from 0 to 1: `fillAtRest`, `switchFillAtRest`, `padFillAtRest`, and `pipeFalloff`. The theme editor shows those four as percentages, so 14 percent in the app is `0.14` in the file. A setting that takes whole numbers drops anything after the decimal point.
- **A number outside its range is ignored, not clamped.** `"cornerRadius": 500` doesn't give you the roundest corners. It gives you Studio Dark's 7.
- **Choices are text,** spelled exactly as this article shows them, capital letters included: `"gradient"`, `"neutralEdge"`, `"filledBar"`. A choice the app doesn't know is ignored.
- **On and off are JSON `true` and `false`,** not `"true"` or `1`.
- **A key the app doesn't know is ignored,** and it's gone the next time the app saves the theme. `_comment` is ignored too. Don't keep notes in a theme file.
- **`name` is required.** It's trimmed at both ends and cut off at 1,024 characters.

> **For agents:** The two mistakes that cost the most time are the byte order mark and the alpha order. A theme saved with a byte order mark never appears in the gallery, and nothing says why. A CSS-style `#RRGGBBAA` color loads without complaint and draws the wrong color. Check every percentage too: `"fillAtRest": 14` is out of range and quietly ignored. You meant `0.14`.

## How each setting changes the page

This section follows the order the app draws in: the deck, the layers over it, and then each part of a control. For each setting it says what the setting is for and where a shipping theme uses it. The key names are the ones in the file; the [key reference](#every-key-in-a-theme-file) also lists the name each one has in the app's theme editor.

### The deck

The deck is the page itself: one color, or a gradient that's lit from above.

```json
"deck": { "kind": "gradient", "color": "#1D1E21", "gradientEndColor": "#07080A", "image": "" }
```

- `kind` is `"solidColor"`, `"gradient"`, or `"image"`.
- A gradient is brightest at the top middle, in `color`, and falls away to `gradientEndColor` at the bottom and the lower corners.
- **A flat color over a whole page reads as a hole rather than a surface.** That's why most themes use a gradient. Studio Dark's deck runs from `#1D1E21` to `#07080A`.
- A painted metal panel isn't lit the way glass is, so Jove and Supersaw use a solid color.
- Bone's deck runs from `#F4EFE4` down to `#E3DAC9` and never darker, because the whole point of Bone is that the space around the controls is bone colored.
- `"image"` is accepted, but this version doesn't draw a picture for it. The deck shows its color instead. To put a picture behind a layout, use the layout's own background picture, which belongs to the layout rather than to the theme.

> **For agents:** If you write a `deck` block, always include `kind`. A deck block without one is a flat color, not a gradient. A deck block without `gradientEndColor` uses `color` for both ends.

> **For agents:** The shipped dark themes work out both ends of the deck from one base color, and you can do the same. For each of red, green, and blue, the top is `base + (255 - base) * 0.07` and the floor is `base * (1 - f)`, where `f = 0.06 + 0.35 * (1 - brightness)` and `brightness = (0.299 * R + 0.587 * G + 0.114 * B) / 255` of the base. Their slot track is `top * 0.45`, and their glass is `base + max(6, (255 - base) * 0.045)`. A theme file doesn't work any of this out for you, so write the results in.

### Over the deck: grain, scan lines, corners, and reflections

These four are laid over the whole page once, so they cost the same with four controls or four hundred. They're all off unless a theme turns them on, and they all live in one `deckOverlay` block.

```json
"deckOverlay": {
  "grainPercent": 34, "grainColor": "#8496B4", "grainStreak": 1,
  "scanLinePitch": 0, "scanLineStrength": 0, "scanLineColor": "#000000",
  "vignettePercent": 0, "vignetteColor": "#00000000",
  "faceplateSheenPercent": 0, "faceplateSheenColor": "#00000000"
}
```

- **Grain** is texture in the panel. It's drawn under the controls, so it never touches them. `grainPercent` is how strong it is: at full it's sandpaper, and at a quarter it's only a warmth. `grainStreak` is how long each speck is. At 1 you get fine specks, like Supersaw's bead-blasted blue panel (34 percent of `#8496B4`). At 40 or more you get streaks running across the page, like Airy System's brushed metal (32 percent of `#FFFFFF`, streak 48). Brushed grain is always lighter than the deck, the way light catches the ridges. An unset grain color is the deck's own color, lifted a little.
- **Scan lines** are the raster of a picture tube: one dark line every few pixels, drawn over everything, controls included. `scanLinePitch` is the distance between lines. It's measured in screen pixels rather than page pixels, so the lines stay the same distance apart and stay sharp at every zoom. `scanLineStrength` is how dark each line is. Cathode, Terminal Amber, and Terminal Green use a pitch of 3 at about 45 percent.
- **Corner fall-off** (`vignettePercent`) darkens the corners, the way a tube is brightest in the middle. It's drawn over the controls. The tube themes use 60. An unset `vignetteColor` is the deck's floor color, taken further down.
- **Faceplate reflection** (`faceplateSheenPercent`) is a soft diagonal light across the upper left: the room, reflected in the glass. It's in every photograph of a real terminal, and it's what makes glass look like glass instead of paint. Terminal Green uses 5 percent of `#BEE1F0`, which is also what an unset color means. It's meant to be faint: at 5 percent, the top left of the page is only a little lighter than the top right, so you won't see it in a close-up.

From the bottom up, the order is: grain, the controls, the corner fall-off, the scan lines, and the reflection.

![Fine grain on Supersaw and brushed grain on Airy System, enlarged three times, and scan lines with corner fall-off in the bottom right corner of Cathode]({{ site.baseurl }}/assets/images/midiglass-theme-decks.png)

### The plate

The plate is the body of a control. A theme makes it in one of three ways, and the app uses the first one that applies:

1. **Tonal.** If the control's fill at rest is above zero, the plate is the deck color washed with the control's own color. `fillAtRest` says how much, from 0 to 1. Tonal Light uses `0.14` and Tonal Dark `0.18`, which is why every control on those themes is lightly colored even when nothing is happening.
2. **A named plate.** If `plateColor` is set, the plate is that color, and `plateEndColor` turns it into a gradient from top to bottom. Bigwig's plate is a raised gray, `#474747` to `#3C3C3C`, because on Bigwig orange only ever means "this is the value." Supersaw's plate is shiny black molding, `#1C1F25` to `#0D0F13`.
3. **Glass.** Otherwise the plate is `glassColor`, laid over the deck at `glassTintPercent` opacity. Studio Dark uses `#17181B` at 86. At 0 there's no plate at all, which is what High contrast does.

**Glass should be a little lighter than the deck, not black.** A black plate on a near-black deck reads as a hole cut in the page rather than an object sitting on it.

A plate can also carry light and shade:

- `plateSheenPercent` washes light down the top of the plate, fading out before the middle, in `plateSheenColor` (white when it isn't set). Studio Dark uses 6. A white sheen on a near-white plate is only haze, so the light themes use 0. On a warm plate a white sheen turns gray, so give a warm theme a warm sheen color.
- `plateShadePercent` darkens the bottom of the plate, in the shadow color. Together with a sheen it makes a plate look glossy, like Jove's colored tabs (sheen 17, shade 16).
- `plateHighlightPercent` draws a one-pixel line of light just inside the top edge, which is what makes plastic look molded. Supersaw uses 24 on its molding. The same line is drawn on fader caps and on note pads.
- `cornerRadius` rounds the corners, in pixels, but never past half the control's shorter side. Studio Dark uses 7, Bigwig 4, Jove 3, the tonal themes 14, and High contrast 0.

Shade and the top-edge line aren't drawn on round controls.

![The same controls drawn as glass on Studio Dark, a tonal fill on Tonal Light, a named plate on Bigwig, glossy tabs on Jove, a shadowed plate on Bone, and black molding on Supersaw]({{ site.baseurl }}/assets/images/midiglass-theme-plates.png)

### Shadows and depth

- `plateElevation` is how dark the shadow under a plate is, from 0 to 100.
- `shadowSpread` is how far the shadow reaches: its blur, in pixels. The shadow also drops down by a third of that.
- `shadowColor` is what the shadow is made of. Black is right on a dark deck. On a light deck a black shadow comes out a dirty gray.
- `thumbShadowPercent` puts a shadow under a fader's cap, so the cap stands off its slot.
- `recessShadePercent` shades the inside of anything cut into the surface: fader and meter slots, and the display wells described below. It's strongest along the top edge.

A shadow is always the exact shape of whatever casts it, and it needs a plate to cast from.

On a dark deck a shadow is barely visible, so Studio Dark leaves it at 55 with a spread of 3. **Bone is where the shadow does the real work.** A warm white plate on a bone deck measures 1.23 : 1, almost no difference at all, so the shadow is the only thing separating a control from the page. Bone uses elevation 45, a spread of 9, and a warm shadow, `#5E5139`. The theme editor warns you when a plate and its deck are this close, because at zero the layout disappears.

A picture tube has no thickness, so the tube themes use no shadow at all.

### The rim

- `rim` says where the outline's color comes from: `"controlHue"` (each control's own color), `"neutralEdge"` (one color for every control, from `neutralRimColor`), or `"none"`.
- `rimStrengthPercent` is how strong a colored rim is at rest. 0 means no rim.
- `switchRimStrengthPercent` does the same for everything you press or read rather than turn or slide: buttons, toggles, pads, page tabs, lamps, meters, readouts, and the like. `-1` means "the same as `rimStrengthPercent`."

Under a finger, the rim rises to nearly full strength. When a switch turns on, its rim lights in the control's color, or on a theme that uses lamps, in a faint line of the lamp's color.

A quarter strength keeps a busy page calm, and Studio Dark uses 28. **On a light plate a faint hairline disappears,** and the control's color goes with it: six faders on six different slots all look the same. Bone runs its rim at 85 for that reason, and the app's accessibility check asks for at least 50 on a light plate. The tube themes use 40 to 55.

Bigwig uses a neutral edge, `#1A1A1A`, so that color only ever means the value. Jove and Supersaw have no rim at all, because on real hardware the control is simply the thing you touch. Airy System lights its knob and fader rims at 85 but sets the switch rim to 0, so its buttons stay black until they're on.

### Light: the glow, the resting glow, and touch

- `glowStrength` is how bright a control's glow gets when you touch it, when MIDI arrives for it, or while a switch is on. A switch that stays on holds a little over half of it. The glow is a soft blur in the shape of the plate.
- `bloomColor` is the color of the glow. When it isn't set, each control glows in its own color.
- `restingGlowPercent` is a floor under the glow, so a control spills a little light all the time. It's a share of `glowStrength`, so it does nothing when the glow is 0. `switchRestingGlowPercent` sets a different floor for everything you press or read, and `-1` means the same.
- `persistenceMilliseconds` is how long the glow takes to fade after a touch. 0 means the normal 220 milliseconds. A control that listens for MIDI has its own "stays lit for" time, 120 milliseconds unless the layout changes it, and that's what MIDI arriving for it uses. The theme's time applies there only when the control's own time is 0.
- `touchFillPercent` washes the plate with the control's color while a finger is on it. On a flat theme with no glow, it's the only thing that shows a control is being held. Switches never take it, because a switch shows it's held by being on.

The glow also drives the halos: the light around a fader's fill, around an XY pad's puck, around a knob's arc on a theme that asks for it, and around a lit round lamp. A theme with no glow has no halos.

When Windows animation effects are turned off, the glow switches on and off instead of fading.

**The tube themes are where these settings matter most.** A box drawn on a picture tube is barely brighter than the glass around it, 1.11 : 1 to 1.22 : 1, so the light it spills all the time is what makes it an object. Cathode, Terminal Amber, and Terminal Green each keep a resting glow (13, 16, and 9 percent), and each glows in its phosphor's color rather than in the control's. Cathode's black and white tube glows blue white, `#CFE0F3`. Terminal Amber glows `#FF6010`, redder than the amber itself, because that phosphor fades through red, and that's what people recognize as an amber screen. Terminal Green glows yellower than its green, `#86F260`. Terminal Amber also holds its glow for 400 milliseconds, the way that phosphor does. On a theme with a resting glow, the theme editor reminds you that at zero the controls would dissolve into the deck.

`lightSource` is an older setting. When `bloomColor` isn't set, `"white"` makes every glow white. It's how Bone lights up: its plates are already near white, so the only room left to say "this one just did something" is to take them the rest of the way. New themes should use `bloomColor` instead.

![Resting glow, and a glow in each phosphor's own color, on Cathode, Terminal Amber, and Terminal Green]({{ site.baseurl }}/assets/images/midiglass-theme-light.png)

### The value

The value is the light that shows where a control is set.

- `valueColor` draws every value on the page in one color: knob arcs, fader fills, pucks, the LFO's wave, and the numbers. When it isn't set, each value is its own control's color. Jove draws every value in its orange, `#E8601C`, and Five-iSH in its white print, `#F2EFE3`.
- `trackColor` is the empty part of a fader or meter slot. A slot is cut into the plate, so it has to be darker than the plate.
- `arcTrackColor` is the empty part of a knob's arc. The arc sits outside the plate on the bare deck, where on a dark theme there's nothing left to be darker than, so it usually has to be a faint light instead. Jove and Supersaw use white at 10 percent, `#1AFFFFFF`. When it isn't set, it's the same as `trackColor`. Without a visible arc track, a knob shows where it's set but never how far it can go.
- `arcTrackHuePercent` makes the empty part of the arc the control's own color at this strength instead. Airy System uses 30, so a green knob sits in a dim green ring.
- `arcGlow` gives a knob's arc the same halo a fader's fill has. Airy System turns it on.
- `pipeFalloff` is how bright the far end of a fader's fill is, from 0 to 1, so the fill is brightest at the value and fades away behind it. 1 is a flat bar. Studio Dark uses 0.35.
- `valueFadesToLight` runs the far end of the fill into the glow color instead. It's what makes an amber fader look like fire on Terminal Amber.
- `valueStrip` puts a thin line along the `"bottom"` or the `"top"` of buttons, toggles, pads, page tabs, lamps, and readouts, or `"none"` for no line. The line is dim where it's empty and lit up to the control's value, so a switch that's on lights all of it.
- `valueIndicator` draws a knob's arc as a `"solidArc"` or as a `"segmentedLamps"` ring. `lampCount` is how many lamps, and a knob smaller than `minimumLampRingSize` pixels falls back to the solid arc, because below that the lamps run together.
- `meterSlots` picks the slots for a meter's three zones: the first 70 percent of its travel, the next 20, and the top 10. Choose three that are also a rising brightness, and the meter still reads for someone with no color vision. The tube themes use the widest three brightnesses they have.

> **For agents:** `meterSlots` counts from 0, unlike the slot numbers the app shows. The default, `[1, 2, 5]`, is slots 2, 3, and 6. If your sixth slot isn't a warning color, set `meterSlots` so the top zone lands on your red.

### Switches: at rest, when on, and lamps

A switch is anything you press: a button, toggle, pad, page tab, or lamp. How a theme draws switches at rest and when they're on is most of its personality.

At rest:

- `switchFillAtRest` is a tonal fill for switches only, from 0 to 1, and `-1` means the same as `fillAtRest`. Jove sets it to 1, so every tab is solid color at rest while its knobs stay black.
- `padFillAtRest` does the same for pads only. Airy System uses 0.34, so its pads are dim colored plastic while the buttons beside them are black.

When on:

- `fillWhenOnPercent` is how much of the control's color the plate takes when it's on. The default is 34. At 100 the plate becomes the color from top to bottom, which is what Bigwig and Bone do. **At 0 the plate doesn't change at all, and the switch lights a lamp instead,** which is what Supersaw and Five-iSH do. A rack of identical black switches with one lamp lit is easier to read across a room than a rack of colored blocks, because the eye only has to find the bright thing. `padFillWhenOnPercent` sets this for pads only.
- `onLiftPercent` moves a lit plate toward white (above 0) or toward black (below 0). Jove uses 36, so a lit tab goes paler, like colored plastic with a lamp behind it. Bone uses `-16`, so a lit button on its light page gets deeper and its white name still reads.
- `lampColor` is the lamp's color when `fillWhenOnPercent` is 0. When it isn't set, a lamp is its control's own color. A lit lamp puts a faint line of its light around the switch, and a named lamp color also colors the switch's glow. Supersaw lights every switch with one red lamp, `#FF523A`.
- `lampShape` is `"bar"`, a short bar across the top of the switch, or `"dot"`, a round lens. An unlit lamp is still drawn, dimly, so you know where to look for it.
- `namesInsideSwitches` puts a switch's name in its middle, whatever `labels` says for everything else, the way a hardware panel prints names above its knobs but on its buttons. The name changes ink when the switch lights, so it can still be read on a bright fill.
- `neutralCaps` makes a switch or fader on the neutral slot wear the neutral color as its cap. It's how a hardware panel marks one row of controls as belonging together without giving it a color. Five-iSH uses it for one row of cream caps. The theme needs a `neutralColor` for this to do anything.

**The on state paints the plate.** On a theme with no plate at all, like High contrast, a switch shows it's on through its value strip.

![Switches off and on: a light fill on Studio Dark, a full fill with the name inside on Bigwig, solid tabs that go paler on Jove, a bar lamp on Supersaw, a round lamp on Five-iSH, and dark buttons beside colored pads on Airy System]({{ site.baseurl }}/assets/images/midiglass-theme-switches.png)

### Knobs

A knob's plate is only its face. The value arc hangs just outside the face, 2 pixels clear, and runs three quarters of a turn, from about seven o'clock round to five o'clock.

- `knobFaceColor` and `knobFaceEndColor` turn the face into a turned cap: brightest just above the middle, where the light catches it, and darker toward the edge. When they aren't set, the face is the plate. A face needs a plate to sit on, so on a theme with no plate, set `plateColor` too.
- `knobCapColor`, `knobCapEndColor`, and `knobCapSizePercent` put a smaller cap in the middle of the face. The size is a share of the face. Without a cap there's a small dot in the middle instead.
- `pointerColor` is the line that shows which way a knob points. When it isn't set, the pointer is the control's color, or the ink on a theme with a neutral rim. On a real panel every pointer is the same color whatever the knob does, and that's what Jove (`#E8601C`), Supersaw (`#E9ECF2`), and Bigwig (`#E6E6E6`) do.
- `pointerOnCap` prints the pointer on the cap, from its edge toward the middle, the way Five-iSH's white line is printed on its black cap. Without it, the pointer comes out from under the cap.
- `knobTickCount` prints a ring of marks around every knob, outside its arc, in the ink. Supersaw prints 9 and Five-iSH 11.

**A knob keeps its own marks.** A new knob shows five marks of its own. On a theme with `knobTickCount`, those five are printed in the ink, and the theme's count is used on knobs whose own marks are turned off. Knobs smaller than 36 pixels have no room for marks.

An encoder is drawn exactly like a knob.

![Knobs on Studio Dark, Bigwig, Jove, Supersaw, Five-iSH, and Airy System]({{ site.baseurl }}/assets/images/midiglass-theme-knobs.png)

### Faders and meters

- `faderPlate` says what a fader's slot is cut into:
  - `"full"` is a plate the size of the whole control, which is what most themes use.
  - `"strip"` is a narrow strip of molding, 5 pixels wider than the slot on each side, with the cap wider than the strip. Supersaw.
  - `"none"` is no plate and no rim, just the slot cut into the panel. The glow comes from the slot. Jove and Five-iSH.
  - `"frame"` has the strip's shape but is cut into the panel instead of standing on it: no shadow and no sheen, with the rim and the resting glow around the slot and the marks printed outside. Airy System.
- `faderFillPercent` is how strong the fill below the cap is. When the cap's position is the whole value, as on Jove (20) and Five-iSH (16), the slot is only faintly lit. A faint fill also throws much less halo.
- `faderScalePercent` prints the marks beside the slot in the ink at this strength, reaching out toward the control's edges like the scale beside a slider on hardware. At 0 they're the faint marks meant for inside a plate.
- `thumb` is the cap: `"none"`, `"neutral"` (a cap in `thumbColor` fading to `thumbEndColor`, with a line through it), or `"hue"` (the cap is the control's color, with no line). The tonal themes and High contrast use colored caps.
- `capLineColor` is the line across a neutral cap. When it isn't set, the line is the control's color, which is what tells six side-by-side faders apart. `capLineWide` makes it nearly the cap's full width and 3 pixels thick, the way hardware paints it. Jove, Five-iSH, and Airy System draw it white on every fader.

A cap is 80 percent of the fader's width, a little over half that tall, and never more than about a third of the travel. A fader narrower than 34 pixels has no cap and shows its fill alone. A fader drawn wider than it is tall is a horizontal fader.

A meter is drawn like a fader without a cap. Its bar takes its colors from the three zones in `meterSlots`, and the zones belong to the slot rather than to the bar, so a rising bar uncovers more of the ramp, like a row of lights.

![Faders with a full plate on Studio Dark, a strip on Supersaw, no plate on Jove, a lit frame on Airy System, a printed scale on Five-iSH, and a fill that fades to the glow on Terminal Amber]({{ site.baseurl }}/assets/images/midiglass-theme-faders.png)

### Displays: wells

- `wellColor` sinks the field of anything that shows you something into its plate: the XY pad's field, the LFO's wave, and the ribbon's strip. When it isn't set, there's no well.
- `recessShadePercent` shades the inside of a well as well as a slot.

This is the "sunk" part of raised, sunk, and flat. Bigwig's wells are `#161616`, darker than every gray around them. Bone's is `#EDE6D8`, with a warm shade along the top.

> **For agents:** A well, a slot, or anything else that's cut in must be darker than whatever it's cut into. A well that's lighter than its plate reads as another raised plate.

### Labels and ink

- `labels` says where a control's name goes: `"above"`, `"below"`, `"inside"` (along the bottom, inside the control), or `"none"`. Hardware panels print names above their controls, which is why Jove, Supersaw, and Five-iSH use `"above"`. A layout can still move any one control's label.
- `inkColor` is the color of labels, tick marks, and printed scales. When it isn't set, the app looks at what's behind each label and picks a light or a dark ink that reads on it. A tube theme names its ink because its ink is its phosphor: a plain white label on amber glass looks like a fault.
- `sectionInkColor` is the ink for anything printed on a filled section. Anything on the deck or on an inner section keeps `inkColor`. Use it when no one ink reads on both. Five-iSH's white print measures only 1.5 : 1 on its tan sections, so the sections are printed in black, `#1D1D1A`.

Which ink a label gets depends on where the words land, not where the control is. A name above a knob can sit on the deck while the knob sits on a section.

A name inside a switch uses the theme's ink only when it reads there, at 4.5 : 1 or better. Otherwise the app picks a light or a dark ink for it, separately for the switch at rest and lit.

Labels are Segoe UI Variable Text at 12 pixels unless the layout sets something else, and the small value numbers are Cascadia Mono at 10 pixels, in the value color. A theme can't change either one.

### Sections, inner sections, and lines

A **Group** control draws a frame around controls that belong together. It sends nothing, and taps go through it to whatever is underneath. The theme calls it a section.

**A Group you add in the editor starts with the Outline style,** so it's only a frame in the rim color, whatever the theme says. Give it the **Plate** style in the inspector and it follows the theme's section settings below: its fill, its outline color, and the inner-section color. The section header settings apply either way.

- `panelFill` is `"plate"` (the same plate a control gets), `"color"` (`panelColor`, fading to `panelEndColor` from top to bottom), or `"none"` (an outline only).
- `panelOutlineColor` is the frame's line. When it isn't set, it's the rim.
- `panelElevation` is how hard a section sits above the deck. `-1` means the same as `plateElevation`. A printed section, like Five-iSH's, uses 0.
- `sectionHeader` says how a section shows its name:
  - `"caption"` puts the name at the top left, inside the frame.
  - `"filledBar"` fills a bar across the top in the section's color, and prints the name in whichever of a dark or a light ink reads on it. Jove's orange banners.
  - `"notched"` sets the name in a gap cut into the top line of the frame. Supersaw.
  - `"centered"` centers the name across the top, inside the frame. Five-iSH and Airy System.
- `sectionNameInHue` prints the name in the section's color instead of the ink. Bigwig, Supersaw, and Airy System.
- `insetPanelColor` and `insetPanelEndColor` give an **inner section** its own color. A filled section whose middle sits on another filled section is an inner section, and it's printed flat, with no shadow, like a second layer of ink. Five-iSH's green blocks inside its tan sections work this way.
- On a theme with a resting glow, a section drawn as an outline glows along its line. Airy System's sections do this.

A section's color is the value color when the theme sets one, and otherwise the color of the section's own slot. That's why every banner on Jove is orange, whatever slot its section uses.

A **Line** control is a printed rule, like the lines between groups of sections on a hardware panel.

- `ruleColor` is its color. When it isn't set, it's the ink at a sixth of its strength.
- `ruleFades` fades the line out at both ends instead of stopping it square. Each line in a layout can also set its own color and ends.

![Sections on Studio Dark, Bigwig, Jove, Supersaw, Five-iSH, and Airy System]({{ site.baseurl }}/assets/images/midiglass-theme-sections.png)

### The piano keyboard

- `keyWhiteColor` and `keyBlackColor` are the natural keys and the sharps and flats. When they aren't set, they're a plain white, `#E8EAEE`, and a plain black, `#16191F`. The natural keys are outlined in the dark key color at half strength, so a light key still has an edge on a light page. A pressed key lights in the value color. Each keyboard in a layout can set its own key colors.

A tube has no white and no black, so Cathode draws its keys in its phosphor and its glass: `#CFDDEE` and `#1B231E`.

## How a theme draws each control

The names here are the ones in the MIDI Glass palette. Everything a control draws comes from the settings above; this section says which ones matter for each control, and why the control is drawn the way it is.

A layout can also give one control its own **style**, which the editor shows as four choices. **Plate** follows the theme, and it's what almost every control uses. **Outline** drops the plate, **Solid** fills the plate with the control's own color, and **Bare** drops both the plate and the rim, leaving only the value and the label. Two controls arrive with a style other than Plate: a new Group is an Outline, and new Text is Bare. Apart from those, a style other than Plate is for the odd control that has to stand apart, like a panic button.

### Knob and Encoder

You turn a knob to a value, so its value is light running around it, and the knob itself can look like the hardware it stands in for.

- **Drawn:** the face (the plate, or a turned face), a cap or a center dot, the pointer, the empty arc, the value arc, marks, the rim around the face, the shadow, and the glow. While you're touching it, its value appears as a number under it.
- **Settings that matter:** `knobFaceColor`, `knobCapColor`, `knobCapSizePercent`, `pointerColor`, `pointerOnCap`, `knobTickCount`, `arcTrackColor`, `arcTrackHuePercent`, `arcGlow`, `valueColor`, and `valueIndicator`.

### Fader and Meter

A fader shows its value by how far the light climbs its slot, and its cap is the thing you grab. So the slot is sunk and the cap stands up.

- **Drawn:** the plate (the whole control, a strip, a frame, or none), the slot, the shade inside it, marks or a printed scale, the fill and its halo, and the cap with its line and its shadow. While you're touching it, its value appears as a number.
- **Settings that matter:** `faderPlate`, `faderFillPercent`, `faderScalePercent`, `thumb`, `thumbColor`, `thumbEndColor`, `capLineColor`, `capLineWide`, `thumbShadowPercent`, `recessShadePercent`, `trackColor`, `pipeFalloff`, `valueFadesToLight`, and `neutralCaps`.
- **A meter** only shows a value it receives. It has the same slot and fill, no cap, and colors from `meterSlots`.

### Button, Toggle, Pad, and Page tab

These are switches: on or off. The plate carries the state, and it's the one place a theme is allowed to fill a whole area with color, which is why "on" reads across a room on a page of two hundred controls.

- **Drawn:** the plate, with the switch or pad fill at rest; the rim; the value strip; and the name, inside the switch when the theme asks. When on: the lit plate or the lamp, a rim in the control's color or a line of the lamp's light, and the glow, held at a little over half.
- **Settings that matter:** `switchFillAtRest`, `fillWhenOnPercent`, `onLiftPercent`, `lampColor`, `lampShape`, `namesInsideSwitches`, `switchRimStrengthPercent`, `switchRestingGlowPercent`, `valueStrip`, and `neutralCaps`. For pads, also `padFillAtRest` and `padFillWhenOnPercent`.
- A button and a pad are on while you hold them, and a toggle stays on until you press it again. None of them take the touch wash.

### Lamp

A lamp is a light that MIDI turns on, like an LED on a piece of hardware.

- On a theme that fills its switches, a lamp's plate lights up the same way a switch's does.
- On a theme that says "on" with a lamp (`fillWhenOnPercent` 0), the lamp is a lamp: a bar at the top of its plate or, with round lamps, the whole control becomes a round lens in a dark bezel that throws a halo when it's lit.
- **Settings that matter:** `lampShape`, `lampColor`, `fillWhenOnPercent`, and `glowStrength`.

### Readout, Text, and Image

- A **Readout** shows a value it receives as a strip along one edge of its plate. It uses `valueStrip`.
- **Text** arrives with the **Bare** style: just its words, in the label ink, with no plate or rim. Give it the **Plate** style in the editor and it gets the theme's plate and rim like any other control.
- An **Image** shows a picture or a video on its plate.

### XY pad, Joystick, and Ribbon

These show their value inside a field rather than along a track.

- An **XY pad**'s field is sunk in a well, with a grid of its own marks, a faint crosshair, and a puck of light with a halo.
- A **Joystick** has two rings, and the outer one is drawn in the rim color when the stick springs back to the middle, because that ring is the only thing that tells you it will. Its puck is a small cap in the fader cap colors, with a dot of the value color in it. With `thumb` set to `"none"`, only the dot shows.
- A **Ribbon** is a strip sunk in a well, with a soft band of light that follows your finger. At rest the band is dim, and it comes right up under a finger.
- **Settings that matter:** `wellColor`, `recessShadePercent`, `valueColor`, `glowStrength`, the rim, and, for the joystick, `thumbColor` and `thumbEndColor`.

### LFO, Beat clock, and Stopwatch

These show something that's running rather than a position.

- An **LFO** has a well, a line across its middle, one cycle of its wave in the value color, and a bead that's dim until it runs. An LFO is never filled like a switch, because a wave the same color as the plate under it can't be seen.
- A **Beat clock** has a ring in the track color, a sweep in the value color that goes around once a bar, a disc that flashes on each beat, and four dots for the beats.
- A **Stopwatch** is its plate and the time, in the value color.

### Turntable

A platter you push with a finger. It has a face in the track color with a rim, a ring of grip marks, a spindle in the fader cap color, and a marker in the value color that turns as you push it. The marker is the only part that moves, which is what shows the platter has been pushed.

### Keyboard

The keys fill the whole control, so its plate barely shows. It uses `keyWhiteColor`, `keyBlackColor`, and the value color for a pressed key.

### Note pads and Hex pads

A grid of pads that each play a note, colored by whether that note is in the chosen key. On these the colors are the information, because they're how a player finds the key without looking for it, so they're stronger at rest than anything else on the page.

- **Drawn:** each pad is a small plate, with the theme's sheen at the top, its shade at the bottom, its top-edge line on square pads, and its corner rounding, up to 18 percent of a pad's width. The whole grid casts one shared shadow, and pads get rims only on a theme that rims its controls. The frame under the pads never lights.
- **Colors:** pads in the key use the control's slot. The key's root uses the slot three along, so slot 1's root is slot 4. Pads outside the key use the neutral color, or the ink on a theme with no neutral. The app strengthens the in-key and root pads until the three are clearly apart, and chooses a note-name ink that reads at 4.5 : 1.

### Group and Line

These are covered in [Sections, inner sections, and lines](#sections-inner-sections-and-lines). Neither sends anything or takes any input.

### Any control whose device is missing

When a control's device isn't connected, the control is dimmed and crossed with faint diagonal lines, and it sends nothing until the device comes back. A theme can't change how that looks.

## The shipping themes

Studio Dark comes first in the gallery because it's the default and the one that stays readable on the busiest page. The rest are in alphabetical order.

| Theme | The idea | Worth borrowing | What it costs |
| --- | --- | --- | --- |
| **Studio Dark** | Smoked glass plates on a near-black deck that's lit from above. Faint rims, and only values and activity are bright. | Glass at 86 percent, rims at 28, a value that fades behind itself (0.35). | Nothing the app warns about. It's the best choice for a dense page. |
| **Airy System** | Black brushed metal and green light, after Roland's AIRA gear. What you turn or slide is lit all the time, buttons are black until they're on, and pads are dim colored plastic. | Brushed grain (streak 48), rims at 85 with a resting glow of 30, switch rim and glow at 0, pads as their own family, knob rings in their own color, arc glow, lit frames around fader slots. | A light show on purpose. On a busy page a touch has less to stand out against. It's made for a dark room. |
| **Bigwig** | A ladder of grays with one lead orange. Color only ever means the value, the state, or which section this is. | A neutral edge instead of colored rims, named plates, sections in their own gray with names in their color, names on switches, a full fill when on, turned knob faces, dark wells. | Red is for light, not words: it's too dark to read as a name. |
| **Blueprint** | Six blues on a navy deck. | A one-color family on Studio Dark's glass. | Its closest two slots, 2 and 4, are only about 11 apart in color, so pair color with position and names. |
| **Bone** | A warm light theme in two colors, bone and a warm brown. The shadow is what separates a control from the page. | A warm shadow (elevation 45, spread 9), rims at 85, a white glow, lit buttons that deepen (-16), sunk slots and wells, cap shadows. | In a dark room it's a lamp pointed at the performer. Best at a desk or in a lit room. |
| **Cathode** | An old black and white TV: olive glass, a blue-white phosphor, soft edges, and no pure black or white anywhere. | Scan lines, corner fall-off, a named plate gradient, a resting glow, a glow color, an ink that is the phosphor, and a fill that fades to the glow. | Its six slots are six brightnesses of one phosphor, so nothing can be grouped by color. Controls are known by where they are and what they're called. |
| **Daylight** | The light version of the original set: a near-white deck with dark, saturated colors. | A named light track, glass at 92, a softer glow (35), a white cap. | Like any light theme, it's bright in a dark room. |
| **Five-iSH** | Black metal with the panel printed on it twice, after the Roland SH-7: tan sections named in black, green blocks named in white, silver knobs with black caps. | Inner sections, a section ink, centered names, pointers on caps, printed scales, round lamps, cream caps on the neutral slot, every value in the print white. | Color is only a lamp, so knobs and faders can't be grouped by color. Put controls on the green or the black, not the tan. |
| **High contrast** | Hard edges, no glow, no rounding, and no plate. Touching a knob or a fader fills it hard. | What to turn off for someone who can't resolve a blur. | Offered, never forced. If Windows switches to high contrast during a set, the running layout is left alone until it's next opened. |
| **Jove** | Matte black steel, orange pointers, and a row of solid colored tabs, after the Roland Jupiter-8. | Switch fill at rest of 1 with bare knobs, tabs that go paler when lit, filled section banners, labels above, no rims, fader slots cut straight into the panel, one value color. | The tabs are full color all the time. That's lovely on a few dozen switches and a wall of color on a few hundred. |
| **Neon Booth** | Saturated neon on a near-black purple deck. | Studio Dark's machinery with louder colors. | Nothing the app warns about. |
| **Supersaw** | A matte blue panel with a fine grain, shiny black molding, and one small red lamp per switch, after the Roland JP-8000. | Fine grain, lamps instead of fills, knob caps and printed mark rings, strip faders, names set in a notch in the section frame. | A switch that's on looks just like one that's off apart from its lamp, so the state is one small light. |
| **Terminal Amber** | An amber terminal: warm maroon glass, a glow redder than the amber, and colors that run like heat from ember to white hot. | A heat ramp that's also a rising brightness, a 400 millisecond glow, a fill that fades to the glow. | A lamp in a dark room. The deepest ember is light rather than letters: give it a rim, a value, or a fill, never a label. |
| **Terminal Green** | A green terminal. The glass isn't green: it's a cool blue slate, with the room reflected across its upper left. | A faceplate reflection, a glow yellower than the green. | The deepest green is too dark to carry a name on a lit control. |
| **Tonal Dark** | Flat, rounded, and friendly, with each control washed in its own color at rest, on a dark deck. | Fill at rest (0.18), a touch fill, colored caps, no glass and no glow. | A busy page is colorful even when nothing is happening, so activity has less room to stand out. |
| **Tonal Light** | The same idea on a near-white deck, with darker colors so a thin rim still reads. | Fill at rest (0.14), a light track, round corners (14). | The same as Tonal Dark. |

## Designing a theme for someone else

This part is for anyone building a theme for someone else, and it's written with AI agents in mind.

### What to ask first

1. **Where will it be played?** A dark stage, a lit studio, a classroom, a desk by a window. This decides between a dark and a light theme before anything else.
2. **What should it remind them of?** A piece of hardware, an app, a brand, a mood. Ask for photos or color codes. A photo of real hardware is the best brief there is.
3. **How busy are their pages?** A few big pads can take strong color at rest. A hundred small controls need a calm page where only activity is bright.
4. **What do their colors mean now?** Which slot is drums, which is the DAW, which is effects. Keep those jobs.
5. **How should a button look when it's on?** A colored block, a lamp, or a solid tab that brightens.
6. **Does anyone who plays it have trouble telling colors apart?** If so, make the slots a rising brightness, and say plainly what the theme can't do.

### Start from the nearest shipping theme

| If they want | Start from |
| --- | --- |
| A calm dark surface for a busy stage page | Studio Dark |
| Loud color on black | Neon Booth |
| A light theme for a bright room | Daylight, or Bone for something warm |
| Flat, friendly, and colorful | Tonal Light or Tonal Dark |
| A DAW or plug-in look: grays and one accent | Bigwig |
| A retro screen | Cathode, Terminal Amber, or Terminal Green |
| A synthesizer panel with colored buttons | Jove |
| Black knobs, lamps, and a textured panel | Supersaw |
| A printed panel with sections | Five-iSH |
| Everything lit up, for a dark room | Airy System |
| The easiest possible reading | High contrast |

Borrow the settings the table above lists for that theme, then change the colors and the deck. The [complete file](#a-complete-theme-file) at the end of this article has every key at Studio Dark's values, so it's a safe place to start typing.

### Measure the colors

Don't judge colors by eye, and don't trust a screenshot to judge them for you. Measure them.

- **Contrast ratio** compares brightness, from 1 : 1 (no difference) to 21 : 1 (black on white). Use the WCAG formula; the PowerShell below does it.
- **3 : 1 or better** for anything that's a line, a bar, a lamp, or a fill: every slot against the deck's top color, the deck's floor, the plate, and any section color it will sit on. This is the same bar the app's slot list and its accessibility check use.
- **4.5 : 1 or better** for anything that carries words or numbers: labels against the deck and the plate, and any slot that prints a name, such as `sectionNameInHue`, a filled bar, or `namesInsideSwitches` with a full fill.
- **Tell slots apart by color difference, not contrast.** Contrast only counts brightness, so it scores two different colors of the same brightness as identical. Use the CIE color difference, ΔE. About 2.3 is the smallest difference anyone can see. Between any two slots somebody has to tell apart on a stage, aim for well over 10. Cathode's neighbors are only 5 to 7 apart, which is why it can't group anything by color.
- **Make the slots a rising brightness as well as different colors** if you can. Then someone with no color vision still gets a ladder of brightness instead of six identical grays.
- **Anything cut in is darker than what it's cut into,** and glass is a little lighter than the deck under it.
- If a color is too dark to carry words, keep it for rims, values, and lamps, and tell the customer, the way the shipping themes' cautions do.
- A theme doesn't have to suit everyone. It does have to be honest about who it doesn't suit. High contrast is always one choice away in the gallery.

```powershell
function Get-Luminance([string]$Hex) {
    $digits = $Hex.TrimStart('#')
    if ($digits.Length -eq 8) { $digits = $digits.Substring(2) }   # alpha comes first; drop it
    $linear = foreach ($at in 0, 2, 4) {
        $v = [Convert]::ToInt32($digits.Substring($at, 2), 16) / 255.0
        if ($v -le 0.03928) { $v / 12.92 } else { [Math]::Pow(($v + 0.055) / 1.055, 2.4) }
    }
    0.2126 * $linear[0] + 0.7152 * $linear[1] + 0.0722 * $linear[2]
}

function Get-ContrastRatio([string]$First, [string]$Second) {
    $one = Get-Luminance $First
    $two = Get-Luminance $Second
    ([Math]::Max($one, $two) + 0.05) / ([Math]::Min($one, $two) + 0.05)
}

Get-ContrastRatio '#FF6B84' '#1E2B33'   # 5.31
```

A translucent plate lands somewhere between its own color and the deck's. To measure against it, mix the two first: each channel is `deck + (plate - deck) * tint / 100`.

### Decide what's lit at rest, and what "on" looks like

Most of a theme's character comes from these choices. Pick one for each kind of control:

| The look | Settings |
| --- | --- |
| Calm at rest, color only on activity | Glass or a named plate, `rimStrengthPercent` near 28, `restingGlowPercent` 0, `fillWhenOnPercent` 34 |
| Colored blocks that light up when on | `fillWhenOnPercent` 100 and `namesInsideSwitches` true. Add `onLiftPercent` above 0 to make a lit switch paler, or below 0 to make it deeper so a light name still reads |
| Lamps instead of fills | `fillWhenOnPercent` 0, `lampColor`, `lampShape`, and `valueStrip` `"none"`, because a strip and a lamp would be two lights saying one thing |
| Solid tabs at rest, bare knobs | `switchFillAtRest` 1, `fillAtRest` 0, a named `plateColor` for the knobs |
| Knobs and faders lit all the time, buttons dark | `rimStrengthPercent` high, `restingGlowPercent` above 0, `switchRimStrengthPercent` 0, `switchRestingGlowPercent` 0 |
| Pads as their own family | `padFillAtRest` and `padFillWhenOnPercent`, independent of the buttons |

### Write and install the file

Save the file as UTF-8 without a byte order mark, in the customer's Themes folder, named after the theme. In PowerShell 7:

```powershell
$folder = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'MIDI Layouts\Themes'
New-Item -ItemType Directory -Path $folder -Force | Out-Null

$path = Join-Path $folder 'Harbor.miditheme.json'
[IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))   # $false: no byte order mark

# Check it the way the app reads it. ReadAllText hides a byte order mark, so look at the bytes.
$bytes = [IO.File]::ReadAllBytes($path)
if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { throw 'The file starts with a byte order mark.' }
$null = [System.Text.Json.JsonDocument]::Parse([IO.File]::ReadAllText($path))   # throws on a comment or a trailing comma
```

`GetFolderPath('MyDocuments')` finds the Documents folder even when it has been moved into OneDrive. Windows PowerShell 5.1's `Set-Content -Encoding UTF8` and .NET's `[Text.Encoding]::UTF8` both write a byte order mark, so don't use them for a theme file.

Then tell the customer how to use it: open a layout in MIDI Glass, go to the layout's **Appearance** settings, and pick the theme in the gallery.

> **For agents:** Don't edit a layout file to add a `themeColors` block while that layout is open in MIDI Glass. The app saves its own copy of the layout and can write over your change. A theme file in the Themes folder is always the safer delivery.

### Check it

You probably can't see the result yourself, so ask the customer to check it, or to send you a screenshot at actual size. The best test page has one of every control, all six slots, a switch that's on and one that's off, a meter near the top of its travel, a section with an inner section inside it (both Groups set to the **Plate** style), and a line.

- Does every control stand out from the deck, including in the corners and at the bottom of a gradient?
- Can they tell all six slots apart, from where they'll actually stand?
- Can they read every label, on the deck, on the plates, and on every section?
- Does a switch that's on look clearly different from one that's off, without needing to see the glow?
- Does a touch show? Does incoming MIDI show?
- Does it still work in the room they'll play in, with the lights they'll have?

The layout's **Accessibility check** page in the editor measures the slots against the deck, checks that a plate that's close to its deck has a shadow or a glow to separate it, and checks the rim on a light plate. The **Appearance** page shows each slot's contrast against the deck, and warns when one is too close.

### Mistakes that are easy to miss

> **For agents:** Each of these loads without an error and gives the customer the wrong theme.
>
> - The file has a byte order mark, so the theme never shows up.
> - A color is written CSS style, `#RRGGBBAA`. MIDI Glass reads it as `#AARRGGBB`.
> - A fraction is written as a percentage: `"fillAtRest": 14` instead of `0.14`. Out of range, so it's ignored.
> - Any number out of range is ignored, not clamped. `"shadowSpread": 100` is Studio Dark's 3.
> - A number or a `true` is written in quotes.
> - A `deck` block has no `kind`, so the gradient is flat.
> - The deck uses `"image"`, which draws only the deck color.
> - The theme is named after a built-in theme, or after Pigment Light, Pigment Dark, or Amber Console, so it's hidden.
> - Two theme files have the same `name`, so one is hidden.
> - A setting is left out on purpose, forgetting that it then takes Studio Dark's value rather than "nothing."
> - `plateEndColor` is set without `plateColor`, or `knobFaceColor` on a theme with no plate. Neither does anything.
> - `restingGlowPercent` is set with `glowStrength` at 0. A resting glow is a share of the glow, so there's none.
> - `lampColor` is set while `fillWhenOnPercent` is above 0. Lamps only light when the fill is 0.
> - `panelColor` is set while `panelFill` isn't `"color"`, or `insetPanelColor` is set while sections aren't filled.
> - The customer's Groups still have the **Outline** style they arrive with, so none of the section settings show. Ask them to set their Groups to **Plate**.
> - `meterSlots` counted from 1 instead of 0.
> - The theme is expected to set fonts, sizes, positions, a background picture, or one control's color. A theme can't do any of those.

## Every key in a theme file

Keys are in the order MIDI Glass writes them. **If left out** is the value the app uses when a file doesn't have the key, which is Studio Dark's. A color shown as `#00000000` is "not set," and the last column says what the app does instead. **In the app** is the matching row on the layout's **Appearance** page. Where the app's slider stops short of what a file can hold, the slider's range is in parentheses. Keep to that range so the customer can still edit the theme in the app.

| Key | Values | If left out | In the app | What it does |
| --- | --- | --- | --- | --- |
| `_comment` | text | | | Ignored. The app writes a line here saying what the file is. |
| `fileVersion` | 1 | 1 | | The file format version. Write 1. A file with a higher number still loads. |
| `name` | text | required | (the name you save under) | The theme's name in the gallery. It can't be a built-in theme's name, or Pigment Light, Pigment Dark, or Amber Console. |
| `hueSlots` | six colors | `#4FC3F7`, `#81C784`, `#FFC247`, `#FF7043`, `#BA68C8`, `#4DD0E1` | Colors | Slots 1 to 6. A missing or unreadable entry keeps Studio Dark's color for that slot. |
| `deck.kind` | `solidColor`, `gradient`, `image` | `gradient` with no `deck` block; `solidColor` in a block without it | Background | A flat deck, a deck lit from above, or a picture. A picture isn't drawn in this version; the deck color is used. |
| `deck.color` | color | `#1D1E21` | Lit top | The deck color, or the top of the gradient. |
| `deck.gradientEndColor` | color | `#07080A`; `deck.color` in a block without it | Shaded floor | The bottom and lower corners of a gradient. |
| `deck.image` | file name | empty | | Not used in this version. Only a plain file name is accepted. |
| `cornerRadius` | 0 to 128 pixels (0 to 32) | 7 | Corner rounding | How round plates, sections, and note pads are. Never more than half a control's shorter side. |
| `glassTintPercent` | 0 to 100 | 86 | Glass tint | How solid a glass plate is. 0 is no plate at all. Only used when there's no `plateColor` and no fill at rest. |
| `glowStrength` | 0 to 100 | 60 | Glow | How bright a control glows when touched, when MIDI arrives, or while a switch is on. Also drives every halo. |
| `labels` | `inside`, `below`, `none`, `above` | `below` | Labels | Where control names go by default. |
| `fillAtRest` | 0 to 1 (the app shows a percentage) | 0 | Fill at rest | A wash of each control's own color over the deck, making the plate. Above 0 the theme is tonal. |
| `touchFillPercent` | 0 to 100 | 22 | Fill under a finger | A wash of the control's color while it's touched. Not used on switches. |
| `trackColor` | color | `#0D0D0F` | Slot track | The empty part of a fader or meter slot, a beat clock's ring, and a turntable's face. |
| `inkColor` | color | `#00000000`: measured light or dark ink | Ink | Labels, marks, and printed scales. |
| `plateColor` | color | `#00000000`: glass or tonal plate | Plate | A plate of this color instead of glass. |
| `rim` | `controlHue`, `neutralEdge`, `none` | `controlHue` | Rim from | Where the rim's color comes from. |
| `neutralRimColor` | color | `#5A5A5A` | Neutral rim | The rim on every control when `rim` is `neutralEdge`. Its alpha counts. |
| `valueStrip` | `bottom`, `top`, `none` | `bottom` | Value strip | A thin line along one edge of switches, lamps, and readouts. |
| `valueIndicator` | `solidArc`, `segmentedLamps` | `solidArc` | Knob indicator | A knob's value as a solid arc or a ring of lamps. |
| `lampCount` | 2 to 128 (2 to 64) | 24 | Lamps | How many lamps in a ring of lamps. |
| `minimumLampRingSize` | 8 to 512 pixels (8 to 160) | 48 | Smallest ring | Below this size a knob uses the solid arc instead. |
| `plateSheenPercent` | 0 to 100 | 6 | Sheen | Light down the top of a plate. |
| `plateElevation` | 0 to 100 | 55 | Raised | How dark the shadow under a plate is. |
| `shadowSpread` | 0 to 64 pixels (0 to 32) | 3 | Shadow reach | How far the shadow reaches. It also drops by a third of this. |
| `shadowColor` | color | `#000000` | Shadow color | The color of every shadow and shade: under plates and caps, in slots, and at the bottom of plates. |
| `lightSource` | `controlHue`, `white` | `controlHue` | | Older. With `white`, glows are white when `bloomColor` isn't set. |
| `rimStrengthPercent` | 0 to 100 | 28 | Rim strength | How strong a colored rim is at rest. 0 is no rim. |
| `pipeFalloff` | 0 to 1 (the app shows a percentage) | 0.35 | Value fade | How bright the far end of a fader's fill is. 1 is a flat bar. |
| `thumb` | `none`, `neutral`, `hue` | `neutral` | Fader cap | No cap, a neutral cap with a line, or a cap in the control's color. |
| `thumbColor` | color | `#313945` | Cap top | The top of a neutral cap, and a joystick's puck. |
| `thumbEndColor` | color | `#161A21` | Cap bottom | The bottom of a neutral cap. |
| `glassColor` | color | `#17181B` | Glass color | What a glass plate is made of. Keep it a little lighter than the deck. |
| `bloomColor` | color | `#00000000`: each control's own color | Light color | The color every control glows in. |
| `restingGlowPercent` | 0 to 100 | 0 | Resting glow | A floor under the glow, as a share of `glowStrength`. |
| `persistenceMilliseconds` | 0 to 10000 (0 to 2000) | 0: 220 ms | Stays lit for | How long the glow takes to fade after a touch, and after MIDI on a control whose own hold time is 0. |
| `plateSheenColor` | color | `#00000000`: white | Sheen color | What the sheen and the top-edge line are made of. |
| `plateEndColor` | color | `#00000000`: a flat plate | Plate bottom | The bottom of a named plate. Needs `plateColor`. |
| `arcTrackColor` | color | `#00000000`: `trackColor` | Knob arc track | The empty part of a knob's arc. |
| `valueFadesToLight` | `true`, `false` | `false` | Fade to the light | The far end of a fader's fill runs into the glow color. Needs a glow color: `bloomColor` or a white `lightSource`. |
| `switchFillAtRest` | `-1`, or 0 to 1 (the app shows a percentage) | `-1`: `fillAtRest` | Switch fill at rest | The fill at rest for buttons, toggles, pads, page tabs, and lamps. |
| `fillWhenOnPercent` | 0 to 100 | 34 | Fill when on | How much of its color a switch's plate takes when on. 0 lights a lamp instead. |
| `pointerColor` | color | `#00000000`: the control's color, or the ink with a neutral rim | Knob pointer | Every knob's pointer. |
| `capLineColor` | color | `#00000000`: the control's color | Cap line | The line across every neutral fader cap. |
| `neutralColor` | color | `#00000000`: none, and neutral controls use slot 1 | Neutral color | The one "no color" color. |
| `sectionHeader` | `caption`, `filledBar`, `notched`, `centered` | `caption` | Section header | How a section shows its name. |
| `sectionNameInHue` | `true`, `false` | `false` | Section name in color | A section's name in its color instead of the ink. |
| `panelFill` | `plate`, `color`, `none` | `plate` | Section fill | What fills a section. |
| `panelColor` | color | `#00000000` | Section color | A section's color when `panelFill` is `color`. |
| `panelEndColor` | color | `#00000000`: a flat fill | Section bottom | The bottom of a section's color. |
| `panelOutlineColor` | color | `#00000000`: the rim | Section outline | The line around every section. |
| `knobFaceColor` | color | `#00000000`: the plate | Knob face | A turned knob face, brightest just above the middle. Needs a plate. |
| `knobFaceEndColor` | color | `#00000000`: `knobFaceColor` | Knob face edge | The edge of the knob face. |
| `knobCapColor` | color | `#00000000`: no cap, a center dot | Knob cap | A smaller cap in the middle of the face. |
| `knobCapEndColor` | color | `#00000000`: `knobCapColor` | Knob cap edge | The edge of the cap. |
| `knobCapSizePercent` | 5 to 100 | 28 | Knob cap size | The cap's size, as a share of the face. |
| `knobTickCount` | 0 to 64 (0 to 32) | 0 | Knob marks | A printed ring of marks around knobs, in the ink. Knobs that show their own marks keep their own count. |
| `namesInsideSwitches` | `true`, `false` | `false` | Names on switches | Switch names go in the middle of the switch. |
| `onLiftPercent` | -100 to 100 | 0 | Lift when on | A lit switch toward white (above 0) or black (below 0). |
| `lampColor` | color | `#00000000`: the control's color | Lamp color | The lamp a switch lights when `fillWhenOnPercent` is 0. |
| `plateShadePercent` | 0 to 100 | 0 | Shade | A shade up from the bottom of a plate, in the shadow color. |
| `plateHighlightPercent` | 0 to 100 | 0 | Top edge light | A one-pixel light line inside the top edge of plates, caps, and pads. |
| `faderPlate` | `full`, `strip`, `none`, `frame` | `full` | Fader plate | What a fader's slot is cut into. |
| `faderFillPercent` | 0 to 100 | 100 | Fader fill | How strong a fader's fill is. |
| `valueColor` | color | `#00000000`: each control's own color | Value color | Every value on the page in one color. |
| `recessShadePercent` | 0 to 100 | 0 | Sunk shadow | A shade inside slots and wells, in the shadow color. |
| `wellColor` | color | `#00000000`: no well | Display well | A sunk field for XY pads, LFOs, and ribbons. |
| `thumbShadowPercent` | 0 to 100 | 0 | Cap shadow | A shadow under fader caps. |
| `capLineWide` | `true`, `false` | `false` | Wide cap line | A cap line nearly the cap's width and 3 pixels thick. |
| `keyWhiteColor` | color | `#00000000`: `#E8EAEE` | Natural keys | A keyboard's natural keys. |
| `keyBlackColor` | color | `#00000000`: `#16191F` | Sharp keys | A keyboard's sharps and flats. |
| `insetPanelColor` | color | `#00000000`: like any section | Inner section | The color of a section inside another section, printed flat. |
| `insetPanelEndColor` | color | `#00000000`: `insetPanelColor` | Inner section bottom | The bottom of an inner section. |
| `panelElevation` | `-1`, or 0 to 100 | `-1`: `plateElevation` | Section raised | How dark the shadow under a section is. |
| `sectionInkColor` | color | `#00000000`: `inkColor` | Section ink | The ink for anything printed on a filled section. |
| `pointerOnCap` | `true`, `false` | `false` | Pointer on the cap | The pointer is printed on the knob cap. Needs a cap. |
| `arcGlow` | `true`, `false` | `false` | Knob value glows | A halo around a knob's value arc. |
| `arcTrackHuePercent` | 0 to 100 | 0: `arcTrackColor` | Knob ring in its color | The empty part of a knob's arc in the knob's own color at this strength. |
| `lampShape` | `bar`, `dot` | `bar` | Lamp shape | A bar lamp or a round lens. |
| `switchRimStrengthPercent` | `-1`, or 0 to 100 | `-1`: `rimStrengthPercent` | Switch rim strength | The rim at rest on everything you press or read. |
| `switchRestingGlowPercent` | `-1`, or 0 to 100 | `-1`: `restingGlowPercent` | Switch resting glow | The resting glow on everything you press or read. |
| `padFillAtRest` | `-1`, or 0 to 1 (the app shows a percentage) | `-1`: like other switches | Pad fill at rest | The fill at rest for pads only. |
| `padFillWhenOnPercent` | `-1`, or 0 to 100 | `-1`: like other switches | Pad fill when on | The fill when on for pads only. |
| `neutralCaps` | `true`, `false` | `false` | Neutral caps | Faders and switches on the neutral slot wear the neutral as their cap. Needs `neutralColor`. |
| `faderScalePercent` | 0 to 100 | 0 | Fader scale | Fader marks printed in the ink at this strength. |
| `ruleColor` | color | `#00000000`: the ink at a sixth | Line color | The color of Line controls. |
| `ruleFades` | `true`, `false` | `true` | Lines fade at the ends | Lines fade out at both ends instead of stopping square. |
| `meterSlots` | three slots, 0 to 5 | `[1, 2, 5]` | Signal, Warning, Too loud | The slots for a meter's three zones, counted from 0. |
| `deckOverlay.scanLinePitch` | 0 to 64 screen pixels (0 to 12) | 0 | Scan lines | Distance between scan lines. 0 is off. |
| `deckOverlay.scanLineStrength` | 0 to 100 | 0 | Scan strength | How dark each scan line is. |
| `deckOverlay.scanLineColor` | color | `#000000` | Scan color | What scan lines are made of. |
| `deckOverlay.vignettePercent` | 0 to 100 | 0 | Corner fall-off | How much the corners darken. |
| `deckOverlay.vignetteColor` | color | `#00000000`: the deck floor, darker | Corner color | What the corners darken to. |
| `deckOverlay.faceplateSheenPercent` | 0 to 100 | 0 | Faceplate | A reflection across the upper left of the page. |
| `deckOverlay.faceplateSheenColor` | color | `#00000000`: `#BEE1F0` | Faceplate color | What the reflection is made of. |
| `deckOverlay.grainPercent` | 0 to 100 | 0 | Grain | How strong the panel's texture is. |
| `deckOverlay.grainColor` | color | `#00000000`: the deck color, lifted | Grain color | What the grain is made of. |
| `deckOverlay.grainStreak` | 1 to 64 | 1 | Grain length | 1 is fine specks. Longer is brushed streaks. |

### A complete theme file

This is Studio Dark written out in full, under a new name so it can sit in the Themes folder. Every key is here, so copy it, rename it, and change what you need.

```json
{
  "_comment": "Windows MIDI Glass theme. Written by the MIDI Glass app. The MIDI service does not read this file.",
  "fileVersion": 1,
  "name": "My Studio Dark",
  "hueSlots": [
    "#4FC3F7",
    "#81C784",
    "#FFC247",
    "#FF7043",
    "#BA68C8",
    "#4DD0E1"
  ],
  "deck": {
    "kind": "gradient",
    "color": "#1D1E21",
    "gradientEndColor": "#07080A",
    "image": ""
  },
  "cornerRadius": 7,
  "glassTintPercent": 86,
  "glowStrength": 60,
  "labels": "below",
  "fillAtRest": 0,
  "touchFillPercent": 22,
  "trackColor": "#0D0D0F",
  "inkColor": "#00000000",
  "plateColor": "#00000000",
  "rim": "controlHue",
  "neutralRimColor": "#5A5A5A",
  "valueStrip": "bottom",
  "valueIndicator": "solidArc",
  "lampCount": 24,
  "minimumLampRingSize": 48,
  "plateSheenPercent": 6,
  "plateElevation": 55,
  "shadowSpread": 3,
  "shadowColor": "#000000",
  "lightSource": "controlHue",
  "rimStrengthPercent": 28,
  "pipeFalloff": 0.35,
  "thumb": "neutral",
  "thumbColor": "#313945",
  "thumbEndColor": "#161A21",
  "glassColor": "#17181B",
  "bloomColor": "#00000000",
  "restingGlowPercent": 0,
  "persistenceMilliseconds": 0,
  "plateSheenColor": "#00000000",
  "plateEndColor": "#00000000",
  "arcTrackColor": "#00000000",
  "valueFadesToLight": false,
  "switchFillAtRest": -1,
  "fillWhenOnPercent": 34,
  "pointerColor": "#00000000",
  "capLineColor": "#00000000",
  "neutralColor": "#00000000",
  "sectionHeader": "caption",
  "sectionNameInHue": false,
  "panelFill": "plate",
  "panelColor": "#00000000",
  "panelEndColor": "#00000000",
  "panelOutlineColor": "#00000000",
  "knobFaceColor": "#00000000",
  "knobFaceEndColor": "#00000000",
  "knobCapColor": "#00000000",
  "knobCapEndColor": "#00000000",
  "knobCapSizePercent": 28,
  "knobTickCount": 0,
  "namesInsideSwitches": false,
  "onLiftPercent": 0,
  "lampColor": "#00000000",
  "plateShadePercent": 0,
  "plateHighlightPercent": 0,
  "faderPlate": "full",
  "faderFillPercent": 100,
  "valueColor": "#00000000",
  "recessShadePercent": 0,
  "wellColor": "#00000000",
  "thumbShadowPercent": 0,
  "capLineWide": false,
  "keyWhiteColor": "#00000000",
  "keyBlackColor": "#00000000",
  "insetPanelColor": "#00000000",
  "insetPanelEndColor": "#00000000",
  "panelElevation": -1,
  "sectionInkColor": "#00000000",
  "pointerOnCap": false,
  "arcGlow": false,
  "arcTrackHuePercent": 0,
  "lampShape": "bar",
  "switchRimStrengthPercent": -1,
  "switchRestingGlowPercent": -1,
  "padFillAtRest": -1,
  "padFillWhenOnPercent": -1,
  "neutralCaps": false,
  "faderScalePercent": 0,
  "ruleColor": "#00000000",
  "ruleFades": true,
  "meterSlots": [
    1,
    2,
    5
  ],
  "deckOverlay": {
    "scanLinePitch": 0,
    "scanLineStrength": 0,
    "scanLineColor": "#000000",
    "vignettePercent": 0,
    "vignetteColor": "#00000000",
    "faceplateSheenPercent": 0,
    "faceplateSheenColor": "#00000000",
    "grainPercent": 0,
    "grainColor": "#00000000",
    "grainStreak": 1
  }
}
```
