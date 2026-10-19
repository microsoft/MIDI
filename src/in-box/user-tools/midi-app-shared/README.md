# midi-app-shared

Source shared by the Windows MIDI Services user tools (`midi2monitor`, `midiscratchpad`, and
future ones). It is **not** a library or a component, and **nothing is copied at build time**.

`BeatClockGenerator` is the exception to the "GUI tools" part: it is plain C++ with no XAML
dependency, and `midi-console` compiles it too so `midi endpoint send-beat-clock` and the
MIDI Clock app keep identical timing behavior. A console tool taking one file from here only
needs step 1, 2 and the `$(ProjectDir)` part of step 4 below.

`GeneralMidi` is the same kind of exception: plain C++, no pch, no WinRT. A project that takes it
must set `<PrecompiledHeader>NotUsing</PrecompiledHeader>` on its `ClCompile` entry. MIDI Player
and Windows MIDI Patchbay both use it, so the General MIDI names they show agree.

The generators are shared too. `BeatClockGenerator` and `TimeCodeGenerator` take either an endpoint connection, which is how MIDI Clock and the console use them, or a `GeneratorSink` from `GeneratorSink.h`: a function that gets each message with its timestamp. Windows MIDI Patchbay's clock and time code steps use the sink, so what they send goes through the patch's other steps on the way out. `LfoMessageGenerator` is Windows MIDI Patchbay's LFO step, and sends through a sink the same way. It can follow a clock instead of its own tempo: hand it timing clock, start and song position with `ReceiveClock`.

`LfoWave`, `LfoSweep` and `ChannelVoiceWords` are plain C++ like `GeneralMidi`, so a project marks them `NotUsing` and the unit tests compile them as they ship. `LfoWave` has the LFO shapes, the noise, and the names files use for them. `LfoSweep` says when each sample of a running LFO is due and where it falls in the cycle. `ChannelVoiceWords` builds MIDI 1.0 and MIDI 2.0 channel voice messages, including a single value as a control change, pitch bend, pressure, RPN or NRPN. Windows MIDI Glass and Windows MIDI Patchbay both use all three, so an LFO sweeps the same way in both apps.

`ClockFollower` is plain C++ too. It keeps track of where an incoming MIDI clock has got to between its pulses, including pulses that arrive before the time they play, which is how `LfoMessageGenerator` follows a clock. Only Windows MIDI Patchbay uses it so far.

`FontCatalog` lists the font families a customer can pick: the ones every Windows PC has, or every family installed on this PC. It reads them from DirectWrite, so a project that takes it links `dwrite.lib`. `FontNames.h` is plain C++ and header only: the default family, and the check a family name read from a file has to pass. Windows MIDI Glass uses both for control labels and Windows MIDI Patchbay for annotations, so the two apps offer the same fonts.

`EndpointCatalog` watches the live endpoints and answers "which live endpoint does this saved one
mean". It owns `EndpointMatch`, `EndpointMatchMode` and `LiveEndpoint`, and it matches on criteria
alone, so it never sees an app's own document type. An app that stores endpoints keeps its own
wrapper that pulls the criteria, the mode and the last known display name out of whatever it saved.
Its `.cpp` spells out the WinRT namespace aliases it needs rather than relying on the consuming
app's `pch.h`, so a new project can pick the file up without matching another app's alias list.
Because shared code cannot reach any one app's telemetry, a swallowed exception goes to
`midiapp::SetEndpointErrorHandler`; set it before starting the catalog or the errors are dropped.

`DocumentHandoff` is for a tool that opens files by double-click. When a copy is already running, the new one sends its paths to the running window with `WM_COPYDATA` and exits. The receiving window has to subclass itself to see the message, because XAML does not pass it on.

`AssistantPrompt` is the **Ask an AI assistant** dialog in Windows MIDI Glass and Windows MIDI Patchbay. It shows a starting prompt the customer pastes into the AI assistant they use, and copies it to the clipboard. The app sends nothing anywhere. Each app builds its own prompt text from its own resources.

`MidiCiAppPresence` and `MidiCiProgramListQuery` ask a device for its program list over MIDI-CI Property Exchange, using the `Windows.Devices.Midi2.CapabilityInquiry` API. The presence holds the app's MIDI-CI identifier on one connection and answers Discovery; each app passes its own model number from `MidiDefs.h`. The query borrows the presence's session. MIDI Keyboard and MIDI Sequencer both use them, so they ask a device the same way. MIDI Keyboard keeps its old header names as thin wrappers. Errors go to `midiapp::SetMidiCiErrorHandler`. The file names differ from the API's own `MidiCiProgramList.h` in `src\in-box\Inc`, which a consuming project's include path would otherwise find first.

`ZipArchive` is the one zip reader and writer for every tool, and `Deflate` is the compression underneath it, written from RFC 1951 so no tool needs a compression library or Windows' own `tar.exe`. Both are plain C++ with no pch, so a project marks them `NotUsing`. `ReadStoredZip` and `BuildStoredZip` are the strict, uncompressed form Windows MIDI Glass content packs need: the local headers and the directory have to describe exactly the same files. `ZipReader` opens zips other apps made, including ones that put each file's sizes after its data, and checks every file against its checksum. `ZipWriter` writes a zip file under a temporary name and renames it when it's done, so a failure never leaves half a zip behind. The MIDI Troubleshooting app uses them for its report viewer and its zips. The tests are in the Windows MIDI Glass unit tests (`ZipArchiveTests`), which also check the deflate code against Windows' own MSZIP codec in both directions.

`SpinButtons.h` is header only. `midiapp::UseTouchSpinButtons(box)` makes a `NumberBox` show its Compact spin pop-up only when a finger or a pen uses it, and no spin buttons for a mouse or the keyboard, which still have the mouse wheel and the arrow keys. Customers found the pop-up confusing with a mouse, and it covered what was next to the box. Call it instead of setting `SpinButtonPlacementMode`. The boxes a window's XAML names can go in one call, `midiapp::UseTouchSpinButtons({ FirstBox(), SecondBox() })`, and their XAML says Hidden, so none of them shows the pop-up before that call.

`PopupBackdrop.h` is header only. WinUI gives context menus the acrylic system backdrop, which takes on the window's tint and the color picked in Appearance, but not a `Flyout`, a `ComboBox`'s list, an `AutoSuggestBox`'s suggestions, or a `NumberBox`'s pop-up spin buttons, which keep their own gray fill and look out of place beside a menu. `midiapp::UseMenuBackdrop(flyout)` gives a flyout the menus' backdrop and see-through background. That makes the flyout a window of its own, which isn't kept inside the tool's window, so `midiapp::OpenTowardWindowMiddle(flyout, anchor)` opens it toward the middle of the window rather than off an edge, and `midiapp::AlwaysOpenTowardWindowMiddle(flyout)` does the same each time a flyout on a button opens. The lists and the spin pop-up can't have the backdrop: each is a `Popup` inside its control's template, and WinUI draws such a pop-up's backdrop offset by where the control is in the window, outside the pop-up, which leaves it see-through ([microsoft-ui-xaml#10087](https://github.com/microsoft/microsoft-ui-xaml/issues/10087)). So `midiapp::UseMenuBackdropForLists()`, called as each window starts, fills them with the color the menus show over the window, worked out the way WinUI's acrylic works it out, through the app's dark and light theme dictionaries and by watching focus for controls made before that. `WindowChrome` passes in the window's color each time it applies it, so the fill follows the color picked in Appearance, and high contrast keeps WinUI's own colors. `UseMenuBackdropForLists` also gives the shared Appearance flyout the backdrop. `midiapp::TintToolTips(useCustom, argb)` gives tooltips the color picked in Appearance, lifted a little toward white the way the menus' acrylic lifts it, through the app's dark and light theme dictionaries so high contrast keeps its own; call it as the window starts and when the color changes.

## What a consuming project has to do

There is no `.props` file and no MSBuild import. Each app's `.vcxproj` lists these files with
explicit relative paths, so any build system that reads the project can see them:

1. `<ClCompile Include="..\midi-app-shared\*.cpp" />` — compile the shared sources.
2. `<ClInclude Include="..\midi-app-shared\*.h" />`
3. `<Midl Include="..\midi-app-shared\MidiAppShared.idl" />` — the shared runtime classes have to
   land in **the app's own winmd**.
4. Add `$(ProjectDir)` and `$(ProjectDir)..\midi-app-shared` to `AdditionalIncludeDirectories`
   for both `ClCompile` and `Midl`. `$(ProjectDir)` is needed because the shared `.cpp` files
   include the *app's* `pch.h`.
5. The app's `pch.h` must include `<winrt/MidiAppShared.h>`, `<microsoft.ui.xaml.window.h>` and
   `<winrt/Microsoft.UI.Composition.SystemBackdrops.h>` before the shared headers.

## Why source rather than a .lib or a WinRT component

The XAML markup compiler resolves `x:Bind` types from the consuming app's winmd. A static library
cannot contribute types to a winmd, and a separate WinRT component would mean shipping an extra
binary next to every tool. Sharing the source keeps each tool a single self-contained executable.

## Two namespaces, on purpose

| Namespace | Contains | Declared in |
|---|---|---|
| `MidiAppShared` | WinRT runtime classes used from XAML (`EndpointChoice`, `NamedChoice`) | `MidiAppShared.idl` |
| `midiapp` | plain C++ helpers (`MidiAppSettings`, `WindowChrome`) | ordinary headers |

They are deliberately different. App code lives inside `namespace winrt::<app>::implementation`,
where an unqualified `MidiAppShared::` binds to the **projection** namespace `winrt::MidiAppShared`
rather than to a plain C++ namespace of the same name. Giving the plain C++ helpers their own
name (`midiapp`) removes that trap instead of relying on every use site writing `::MidiAppShared::`.

## The one non-obvious detail

These types are in the `MidiAppShared` namespace, which is **not** the apps' root namespace. When
a runtime class sits outside `$(RootNamespace)`, cppwinrt names the generated headers with the
full namespace:

```
MidiAppShared.EndpointChoice.g.h      not  EndpointChoice.g.h
MidiAppShared.EndpointChoice.g.cpp    not  EndpointChoice.g.cpp
```

The projection is `<winrt/MidiAppShared.h>`; include it in the app's `pch.h` before any namespace
alias for it. Types in the app's own root namespace keep the short generated names, so the two
conventions sit side by side in the same project.
