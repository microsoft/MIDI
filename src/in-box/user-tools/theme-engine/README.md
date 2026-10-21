# Theme engine

A theme engine for the WinUI 3 tools (the MIDI Sequencer first; Studio and Stage can take the
folder as it is). A theme is a JSON file with a light look and a dark look, written in CSS's terms:
colors, `oklch(from tint …)`, gradients, box shadows, borders, radii. The engine resolves it for
light or dark and the user's window color, recolors the app's XAML brushes, and draws "parts"
(clips, launcher pads, keys, readouts) with Win2D the way a browser draws the same CSS, so a theme
ported from an HTML comp looks like the comp.

## Files

| File | What it does | Needs |
|---|---|---|
| `ThemeColor` | Colors: sRGB, linear, OKLab, OKLCH, CSS `color-mix`, contrast | plain C++ |
| `ThemeSyntax` | Reads CSS-like values: colors with names and `from` syntax, `calc`, backgrounds, shadows, lines, corners, edges, filters | plain C++ |
| `ThemeStyle` | A part's look: what's set, and what it resolves to | plain C++ |
| `ThemeDefinition` | Loads and checks a theme file | Windows.Data.Json |
| `ThemeResolver` | A theme for light or dark and a window color: tokens, numbers, metrics, fonts, parts (cached), resources | Windows.Data.Json |
| `ThemeLibrary` | Themes from the app's RCDATA resources | Windows.Data.Json |
| `ThemeXaml` | Sets the theme's brushes in a resource dictionary's theme dictionaries | the app's `pch.h` (WinUI) |
| `ThemePainter` | Draws a part with Win2D: shadows (cached nine-slice blurs), backgrounds, borders, outlines, opacity, filters | the app's `pch.h` (Win2D) |
| `ThemePlates` | Draws parts behind XAML controls, in their states, and gives the controls the part's text color and font | the app's `pch.h` |
| `ThemePixels`, `ThemeCanvas` | Keeps a Win2D canvas at a size it draws steadily at (below) | none; the app's `pch.h` |

The first six compile without the app's precompiled header (`NotUsing`), so unit tests can take
them. The last three include the consuming app's `pch.h`, as the files in `midi-app-shared` do.

## A theme file

```json
{
    "themeEngine": 1,
    "id": "aluminum",
    "name": "Aluminum",
    "description": "…",
    "backdrops": [ "solid" ],
    "windowColor": "material",
    "light": {
        "numbers": { "anodize": 1 },
        "tokens": { "plate": "#D3D3CF", "text": "#1B1B1D", "accent": "#3B6FD6" },
        "tinted": { "plate": "oklch(from tint 0.86 calc(min(c * 0.38, 0.014) * anodize) h)" },
        "trackColors": { "#FF8A65": "#E0672C" },
        "shadows": { "crisp": "inset 0 1px 0 rgba(255, 255, 255, .35), 0 2px 3px rgba(0, 0, 0, .35)" },
        "fonts": { "label": "Bahnschrift SemiCondensed" },
        "metrics": { "transportHeight": 56 },
        "parts": {
            "launcher.cell": {
                "background": "radial-gradient(circle at 35% 30%, #5A5A60, #2A2A2D 72%)",
                "border-top": "2px solid track",
                "box-shadow": "crisp",
                "states": { "playing": { "box-shadow": "pressed, 0 0 10px track" } }
            }
        },
        "resources": { "SeqWindowSurfaceBrush": "plate" }
    },
    "dark": { "extends": "light", "tokens": { "plate": "oklch(0.335 0.003 100)" } }
}
```

- **windowColor**: `material` uses the user's window color as `tint` in `tinted`, which replaces
  tokens of the same name; `backdrop` leaves the color to the window's backdrop (Night drive).
- **Names**: a token, a number, a shadow, `tint` (in `tinted`), `track` (in a part or a track
  token, the track's color as the theme shows it), and only `transparent`, `white` and `black` of
  CSS's named colors. Every name is checked when the file loads, and problems are reported with
  their path, such as `dark.parts.launcher.cell.box-shadow`.
- **Parts**: CSS properties (`background`, `border`, `border-top` …, `border-radius`,
  `box-shadow`, `outline`, `outline-offset`, `opacity`, `filter`, `color`, `font-family`,
  `font-size`, `font-weight`, `width`, `height`, `margin`), plus values of the app's own: a key
  ending in `color` is a color, a length is a number, anything else a word (`"numerals": "lining"`).
  States are merged in the order they're asked for; later ones win.
- **Keys starting with `$`** are comments.
- **dark** can extend **light**; `tinted` isn't inherited.

## ThemeResources and XAML

`XamlThemeResources::Apply` recolors brushes in place, so every `{ThemeResource}` that took one
follows. A key it adds or takes away is only seen after a theme change, so the caller changes the
host's `RequestedTheme` to the other theme and back when `Apply` returns true. Dialogs and pop-ups
aren't under the host, so they keep the app's own resources.

## Plates

A `PlateLayer` is one Win2D canvas at the back of a panel. It redraws when the layout moves an
element; an element moved by a render transform or faded by its opacity needs `Invalidate`. A
plate is drawn as see-through as its element and what holds it. Rows that are built again often
(the Sequencer's track headers) go in a `Group`, taken off together with `Remove` before they're
built again. A key's light (`led-color`, and `led-size`, `led-top`, `led-right`, `led-bottom`,
`led-glow` for where and whether it glows) is the app's own decoration, drawn by `Decorate`.

## Canvases at 125%, 150% and 175%

Text on a Win2D `CanvasControl` that's drawn again and again, as a sequencer's is while it plays,
can shake: by a pixel at some sizes
([microsoft/Win2D#989](https://github.com/microsoft/Win2D/issues/989), open), and by a quarter
pixel at others. The control draws through a translation, to where XAML keeps its picture, that's
exact one time and a hair off the next; shapes don't show that, but Direct2D places glyphs
differently. Two things in `ThemeCanvas.h` stop it, and a canvas drawn often needs both:

- `KeepWholePixels(canvas)` sizes the canvas to the largest whole number of DIPs and pixels that
  fits where layout puts it (`ThemePixels.h`: a multiple of 4 DIPs at 125% and 175%, 2 at 150%),
  at the top left, and keeps it so.
- `SteadyCanvas::Draw(sender, args, draw)`, in the Draw handler, draws into a picture of the
  canvas's own size, where the translation is exact, and copies it over pixel for pixel.

Plate layers do the first themselves; they're drawn seldom, so they don't need the second.

## Taking it into another app

Copy the folder, add its include path, compile the first six files without the precompiled header,
and make sure the app's `pch.h` includes WinUI and Win2D (`Microsoft.Graphics.Canvas`,
`.Effects`, `.UI.Xaml`). The Sequencer's `MainWindowStyle.cpp` and `MainWindowPlates.cpp` show the
app's side: which themes it carries, how it applies them, and which controls get plates.
