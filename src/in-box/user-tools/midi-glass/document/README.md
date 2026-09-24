# The MIDI Glass document layer

The layout document, its file format, the page size templates and the theme model.

## What this is

| File | Holds |
|---|---|
| `LayoutModel.*` | `LayoutDocument` and everything under it: pages, controls, messages, devices, sequences. Plain data, plus `Validate`. |
| `LayoutSerializer.*` | The document to and from `.midilayout.json` text. |
| `LayoutStore.*` | The layouts folder, reading, writing and listing. The only part of this layer that touches a disk. |
| `JsonText.*` | The deterministic writer, and the machinery that keeps fields this build does not understand. |
| `PageTemplates.*` | The page sizes a new layout starts from, and the control sizes derived from them. |
| `ThemeModel.*` | The theme property table, the nine shipped themes, and measured contrast. |
| `ThemeStore.*` | Themes as their own shareable `.miditheme.json` files. |

## The contract

**Reading never throws and never trusts.** A layout can arrive from a stranger, so every string is bounded, every collection is capped, every number is range checked and anything malformed is dropped rather than believed. A file that is not valid JSON fails; a file that is valid JSON but full of nonsense loads with the nonsense removed.

**Writing is deterministic.** The same document always produces the same bytes. This is why the writer is here rather than `JsonObject::Stringify` — a `JsonObject` is a map and does not promise to hand the keys back in the order they went in, so a file written through it can reorder itself on a save where nothing was edited.

**A file this build only partly understands still round-trips.** Unknown keys are kept at every level — document, page, control, message, device, sequence step — and written back. An older build opening a newer layout does not quietly destroy the parts it cannot edit.

**A control stores a hue slot, not a color.** That is what makes a theme swap a six color operation rather than a redesign.

**Devices are matched on the same criteria the service configuration uses**, through `midiapp::EndpointMatch`, so the keys cannot drift from the service and a layout has a real chance of finding the right hardware on another PC.

**A theme that came from a file is never built in**, whatever the file says, and its deck image is a bare file name rather than a path. A theme is the thing people swap on a forum, so it is the one document here most likely to have come from a stranger.

## Invariants

- `sizeof` nothing here is on a hot path. This layer runs when a file is opened or saved, never per frame.
- No file in this folder includes `pch.h`, XAML, or any WinRT UI type. They compile into `Midi2.MidiGlass.unittests` unchanged, which is how the layer is tested without a window and without a device. Both projects mark them `PrecompiledHeader NotUsing`.
- `Validate` returns what is wrong in the order a person would want to fix it. An empty result means the document is safe to run. It never means the document is good.
- Controls outside the page rectangle are reported, never moved. Clamping them would make shrinking a page unrecoverable.

## What this does not do

- **It does not draw anything.** No thumbnail, no preview, no color resolution beyond the contrast measurement. Rendering is the surface layer's job and it does not live here. The card is next door in `thumbnail/`.
- **Only the two stores touch a disk.** The model and the serializers convert between a document and a string and never open a file, which is what lets them be tested without one.
- **Nothing here auto-saves.** Deciding when a document is dirty and when to write it belongs with the editor, which does not exist yet.
- **It does not talk to the MIDI service, open a connection or resolve a device to real hardware.** It holds the criteria; `midiapp::EndpointCatalog` resolves them.
- **It does not know what a control looks like.** `ControlKind` is an identity, not an appearance.
- **It does not enforce validity.** A document can be built and saved with problems in it, because an editor has to let somebody be half way through. `Validate` reports; it does not repair.
- **It has no notion of running.** No values, no state, no time. A layout always starts from its own defaults, so there is nothing here to carry between runs.
