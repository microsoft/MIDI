# The MIDI Glass editor layer

Everything the editor does to a layout, with no XAML anywhere in it.

| File | Holds |
|---|---|
| `EditGeometry.*` | The grid, magnetic guides, resize handles, what is off the page, and the two page-resize paths. |
| `ArrangeOps.*` | Align, distribute and equal gaps. |
| `RepeatPlan.*` | Where the copies of a repeat go, and what changes about each one. |
| `UndoStack.*` | One stack per open layout. |
| `ControlFactory.*` | What a control looks like the moment it lands on the page. |
| `EditorController.*` | One editing session. The only thing that writes to the document. |

**Nothing here includes `pch.h`, XAML or the MIDI SDK**, so the unit tests compile it unchanged. That is the whole reason the editor can be checked without a window: the rules below are tests, not screenshots.

## EditorController

One session owns the document, the selection, the snap settings and the undo stack, and is the only thing that writes to any of them. The window draws what it holds and turns pointers and keys into calls on it.

- **A drag is one undo entry, not a hundred.** `BeginDrag` records where everything started, so every update is measured from there rather than accumulating rounding.
- **Typed bounds are exact and never snapped.** Somebody who types 456 gets 456.
- **A control placed from the keyboard goes to the first free spot**, walking the grid from the top left. Landing every one of them on top of the last is not a usable answer for somebody who cannot click the page.
- **`ControlIndexOf` counts every page in order**, which is how the binding engine and the surface index controls. Per-page numbering would make the monitor rail filter on the wrong thing.

## EditGeometry

- **A guide beats the grid.** Somebody lining a control up with the one beside it means that, not the nearest 8 px.
- **Alt suspends snapping** without changing what the toolbar says, so free placement never costs a setting change.
- **Nothing is ever clamped inside the page.** A control left outside after a shrink is ghosted, counted and still selectable. Clamping is what would make a resize unrecoverable.
- **Grow then shrink back puts every control exactly where it started.** That is a test, because it is the property that makes the page size safe to experiment with.

## What this does not do

- **It does not draw.** The window owns the canvas, the overlay and the handles; this layer says where they go.
- **It does not save.** The window owns the auto-save timer and calls `WriteLayoutFile`.
- **It does not send.** Try mode belongs to `runtime/LivePlayer`, and nothing in this folder knows a device exists.
- **It does not know about resources.** An undo entry carries a resource key, and the window looks it up.
