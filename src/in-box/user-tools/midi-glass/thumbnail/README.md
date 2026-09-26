# The MIDI Glass thumbnail

Draws the card the library shows for a layout.

## Two ways to draw a card

**The real surface, first.** `SurfaceThumbnail.*` builds the layout's first page with the same surface renderer the runtime uses, far off to one side of the library window, captures it with a `RenderTargetBitmap` and writes it as a PNG. A card then shows exactly what the layout looks like when it opens: knob faces, lamps, wells, section frames, labels and all. It used to be a second, simpler drawing of the layout, and every time the surface learned something new the card fell further behind.

**The plain drawing, when the real one cannot be made.** XAML and composition only render into a live window. A capture that fails or comes back empty, and the headless command line below, fall back to `ThumbnailRenderer`, which draws onto an offscreen bitmap with Win2D on a software device. That works on a machine with no usable GPU and with nothing on screen, so a card is never left blank.

| File | Holds |
|---|---|
| `SurfaceThumbnail.*` | Building the page out of sight, capturing it and writing the PNG. Needs the library window, and runs on its thread one layout at a time. |
| `ThumbnailLayout.*` | What the plain card contains: where the page sits, which controls are on it, what color each one is. Arithmetic only — no Win2D, no pch, no XAML, so it is all tested. The real card borrows its letterbox color from here too. |
| `ThumbnailRenderer.*` | The plain drawing onto a bitmap, the PNG, and where cards live. The only part of the plain path that needs a graphics device. |

The library finds stale cards on its reading thread and draws them afterwards on the window's own thread, because a capture has to happen there. A card is stale when its layout file is newer than it is.

## The rules both follow

- **Letterboxed, never stretched.** Same rule the runtime uses, for the same reason: a control surface is muscle memory, and a card that showed different proportions from the real thing would be a lie at exactly the moment somebody is choosing between layouts.
- **Off-page controls are not drawn.** The editor still shows them and they are still real; they are not part of what ships.
- **Nothing on a card can be reached.** The page is on screen for a tenth of a second, far outside the window, with every control taken out of the Tab order and out of what a screen reader sees.
- **Cards live under `%LOCALAPPDATA%`**, not beside the layout. Documents is perfectly writable; the reasons are that the customer looks at that folder and derived files clutter it, that a cache should not be synced or backed up, and that zipping the layouts folder to send to a friend should contain layouts and nothing else. Deleting the whole cache must never lose anything.
- **The way cards are drawn is part of their name.** `ThumbnailCacheVersion` goes into every file name, so raising it makes every card drawn the old way disappear from view and get drawn again. The old files are cleared out once a session.

## Generating one by hand

```
midiglass --thumbnail <layout file> <output png> [width]
```

Returns before any window is created, so it always uses the plain drawing. It is also how the headless path is tested.

## What this does not do

- **It does not know when a card is stale.** Deciding when to draw one again belongs to the library, which owns the cache.
- **It does not read or write layouts.** It is handed a document.
- **It does not pick a theme.** It is handed one.
