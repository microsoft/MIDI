# The MIDI Glass thumbnail

Draws a card for a layout with no window and no compositor.

## Why this exists at all

The library opens on a wall of cards, and a card has to be right for a layout that has **never been opened on this PC** — a file a friend sent, or a layout restored from a backup. So a thumbnail is drawn from the layout model, not captured from a running window.

That is also why this is the one place in the app that draws without XAML. The surface renderer is XAML plus composition, and **neither will produce a frame without a window to produce it into**. Win2D draws onto an offscreen bitmap instead, on a software device, which is what makes this work on a machine with no usable GPU and with nothing on screen.

| File | Holds |
|---|---|
| `ThumbnailLayout.*` | What the card contains: where the page sits, which controls are on it, what color each one is. Arithmetic only — no Win2D, no pch, no XAML, so it is all tested. |
| `ThumbnailRenderer.*` | Putting that on a bitmap and writing the PNG. The only part that needs a graphics device. |

The split is deliberate. Everything that can be wrong about a thumbnail — a stretched page, a control in the wrong place, an off-page control that should not be there, a hue read from the wrong slot — is decided in `ThumbnailLayout` and covered by tests that need no device.

## The rules it follows

- **Letterboxed, never stretched.** Same rule the runtime uses, for the same reason: a control surface is muscle memory, and a card that showed different proportions from the real thing would be a lie at exactly the moment somebody is choosing between layouts.
- **Off-page controls are not drawn.** The editor still shows them and they are still real; they are not part of what ships.
- **A control too small to draw is drawn anyway**, at a minimum size. Rounding it away would make a dense layout look empty.
- **The hue is resolved before the renderer sees it.** The renderer never consults a theme.
- **Cards live under `%LOCALAPPDATA%`**, not beside the layout. Documents is perfectly writable; the reasons are that the customer looks at that folder and derived files clutter it, that a cache should not be synced or backed up, and that zipping the layouts folder to send to a friend should contain layouts and nothing else. Deleting the whole cache must never lose anything.

## Generating one by hand

```
midiglass --thumbnail <layout file> <output png> [width]
```

Returns before any window is created, so it is also how the headless path is tested.

## What this does not do

- **It does not draw the surface.** A card is a suggestion of a layout at 480 x 300, not a small copy of it. There is no text, no label, no value, no state, and thirteen control types collapse to two shapes.
- **It does not know whether a card is stale.** Deciding when to regenerate belongs to whatever owns the cache.
- **It does not read or write layouts.** It is handed a document.
- **It does not pick a theme.** It is handed one.
