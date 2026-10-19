// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A theme file: JSON, one per theme, with a light and a dark look. README.md in this folder
// describes the format; in short:
//
//   {
//     "themeEngine": 1,
//     "id": "tactile", "name": "Tactile", "description": "...",
//     "backdrops": [ "solid" ],          the backdrops it can sit on: solid, mica, acrylic
//     "windowColor": "material",         "material": the window color is "tint" in the theme's own
//                                        colors; "backdrop": the app tints its backdrop with it
//     "light": {
//       "tokens":    { "surface": "#E4E3DF", "text": "#2A2A2D", ... },
//       "tinted":    { "surface": "oklch(from tint 0.915 min(c * 0.45, 0.012) h)", ... },
//       "numbers":   { "glaze": 1 },
//       "shadows":   { "raised-small": "3px 4px 11px oklch(from shade l c h / .18), ..." },
//       "fonts":     { "label": "Bahnschrift" },
//       "metrics":   { "transportHeight": 60 },
//       "parts":     { "launcher.cell": { "background": "...", "box-shadow": "raised-small",
//                                         "states": { "playing": { ... } } } },
//       "resources": { "TextFillColorPrimaryBrush": "text" },
//       "trackColors": { "#FF8A65": "#FF5B1F" }
//     },
//     "dark": { "extends": "light", ... only what's different ... }
//   }
//
// "tinted" replaces tokens when the customer has picked a window color, and is the only place
// "tint" can be used. "track", the color of the track being drawn, can be used anywhere but in
// "resources". A mode that extends the other starts from all of it but its "tinted".

#include <memory>

#include "ThemeStyle.h"

namespace themeengine
{
    constexpr int32_t ThemeFormatVersion = 1;

    struct PartSpec
    {
        StyleSpec Base{};
        std::map<std::wstring, StyleSpec, std::less<>> States{};
    };

    struct ThemeMode
    {
        std::map<std::wstring, ColorExpression, std::less<>> Tokens{};
        std::map<std::wstring, ColorExpression, std::less<>> Tinted{};
        std::map<std::wstring, double, std::less<>> Numbers{};
        std::map<std::wstring, std::vector<ShadowLayer>, std::less<>> Shadows{};
        std::map<std::wstring, std::wstring, std::less<>> Fonts{};
        std::map<std::wstring, double, std::less<>> Metrics{};
        std::map<std::wstring, PartSpec, std::less<>> Parts{};

        // A XAML resource key and the color or gradient its brush gets.
        std::map<std::wstring, BackgroundLayer, std::less<>> Resources{};

        // The app's track colors (0xRRGGBB) as this theme shows them: a material can have its own
        // caps for them. A color with no entry is shown as it is.
        std::map<uint32_t, ColorExpression> TrackColors{};
    };

    enum class WindowColorUse : int32_t
    {
        // The app tints its backdrop (or a solid fill) with the window color, as the MIDI tools
        // always have, and the theme's colors don't change.
        Backdrop,

        // The window color is "tint" in the theme's own colors.
        Material,
    };

    struct ThemeDefinition
    {
        std::wstring Id{};
        std::wstring Name{};
        std::wstring Description{};

        // "solid", "mica" and "acrylic", in the order the theme prefers them.
        std::vector<std::wstring> Backdrops{};

        WindowColorUse WindowColor{ WindowColorUse::Material };

        ThemeMode Light{};
        ThemeMode Dark{};

        ThemeMode const& Mode(_In_ bool dark) const noexcept { return dark ? Dark : Light; }
        bool AllowsBackdrop(_In_ std::wstring_view backdrop) const noexcept;
    };

    struct ThemeLoadResult
    {
        // Set only when the theme loaded with no problems.
        std::shared_ptr<ThemeDefinition const> Theme{};

        // Each one names where it is: "dark.parts.launcher.cell.box-shadow: ...".
        std::vector<std::wstring> Problems{};

        // The file was written for a newer version of the engine.
        bool IsFromNewerVersion{ false };
    };

    ThemeLoadResult LoadTheme(_In_ std::wstring_view json);
}
