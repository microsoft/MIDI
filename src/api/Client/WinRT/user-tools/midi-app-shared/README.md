# midi-app-shared

Source shared by the Windows MIDI Services user tools (`midi2monitor`, `midiscratchpad`, and
future ones). It is **not** a library or a component, and **nothing is copied at build time**.

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
