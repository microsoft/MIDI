// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <string>
#include <string_view>
#include <vector>

#include "ThemeModel.h"

namespace glass
{
    // A theme is its own small file, so one can be saved, reused across layouts, and shared.
    // Same rules as a layout: the platform parses, we write, and anything this build did not
    // understand comes back out again.
    constexpr wchar_t ThemeFolderName[] = L"Themes";
    constexpr wchar_t ThemeFileExtension[] = L".miditheme.json";

    constexpr uint32_t ThemeFileVersion = 1;

    struct ThemeReadResult
    {
        bool Succeeded{ false };
        Theme Value{};
        bool IsFromNewerVersion{ false };
        std::wstring Detail{};
    };

    ThemeReadResult ReadThemeFromJson(_In_ std::wstring_view json) noexcept;
    std::wstring WriteThemeToJson(_In_ Theme const& theme) noexcept;

    // "#RRGGBB", or "#AARRGGBB" when the alpha is not full. A theme is a file people hand-edit
    // and paste colors into, so the colors are written the way they are everywhere else rather
    // than as four numbers.
    std::wstring ColorToText(_In_ ThemeColor const& color) noexcept;
    bool TryParseColor(_In_ std::wstring_view text, _Out_ ThemeColor& color) noexcept;

    // ---- the folder ----

    std::wstring ThemesFolder() noexcept;

    ThemeReadResult ReadThemeFile(_In_ std::wstring const& filePath) noexcept;

    // Refuses to write over one of the nine that ship: a customer who edited "Studio Dark" and
    // then wanted it back would have nothing to go back to. Save a copy under a new name instead.
    bool WriteThemeFile(_In_ Theme const& theme, _In_ std::wstring const& filePath) noexcept;

    std::vector<std::wstring> ListThemeFiles() noexcept;

    // The nine that ship plus everything in the themes folder, built-ins first, so a picker shows
    // one list. A customer theme with the name of a built-in does not replace it.
    std::vector<Theme> AllThemes() noexcept;
}
