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

#include <winrt/Windows.Data.Json.h>

#include "ThemeModel.h"
#include "JsonText.h"

namespace glass
{
    struct LayoutDocument;
    // A theme is its own small file, so one can be saved, reused across layouts, and shared.
    // Same rules as a layout: the platform parses, we write, and a top-level key this build did
    // not understand comes back out again. Unknown keys inside deck and deckOverlay do not.
    constexpr wchar_t ThemeFolderName[] = L"Themes";
    constexpr wchar_t ThemeFileExtension[] = L".miditheme";

    // What the first builds wrote. Still read, and renamed when the app starts.
    constexpr wchar_t LegacyThemeFileExtension[] = L".miditheme.json";

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

    // The same theme, as a block inside somebody else's file rather than as a file of its own.
    //
    // A layout carries the theme it was built with, so a layout sent to a friend looks the way
    // it was built even though they have never seen the theme. These two are what make that one
    // shape rather than two that can drift apart.
    void WriteThemeBody(
        _Inout_ JsonTextWriter& writer,
        _In_ Theme const& theme) noexcept;

    Theme ReadThemeObject(
        _In_ winrt::Windows::Data::Json::JsonObject const& object) noexcept;

    // "#RRGGBB", or "#AARRGGBB" when the alpha is not full. A theme is a file people hand-edit
    // and paste colors into, so the colors are written the way they are everywhere else rather
    // than as four numbers.
    std::wstring ColorToText(_In_ ThemeColor const& color) noexcept;
    bool TryParseColor(_In_ std::wstring_view text, _Out_ ThemeColor& color) noexcept;

    // ---- the folder ----

    std::wstring ThemesFolder() noexcept;

    // A picture a theme names is a bare file name in the themes folder, beside the theme files.
    // Empty when the name is a path, the file is not a png or jpg, or it is not there.
    std::wstring ThemePicturePath(_In_ std::wstring const& fileName) noexcept;

    std::wstring DeckImagePath(_In_ ThemeDeck const& deck) noexcept;

    // Copies a picture into the themes folder for a deck. The file name to store, or empty.
    std::wstring CopyDeckImageToThemes(_In_ std::wstring const& sourcePath) noexcept;

    ThemeReadResult ReadThemeFile(_In_ std::wstring const& filePath) noexcept;

    // Refuses to write over one of the ones that ship: a customer who edited "Studio Dark" and
    // then wanted it back would have nothing to go back to. Save a copy under a new name instead.
    bool WriteThemeFile(_In_ Theme const& theme, _In_ std::wstring const& filePath) noexcept;

    std::vector<std::wstring> ListThemeFiles() noexcept;

    // The ones that ship plus everything in the themes folder, built-ins first, so a picker shows
    // one list. A customer theme with the name of a built-in does not replace it.
    std::vector<Theme> AllThemes() noexcept;

    // The theme a layout is actually drawn with: the one it carries, or the one it names, or the
    // default. One place, because four windows resolve this and they have to agree - a layout
    // that looked one way in the editor and another way running is the worst kind of defect to
    // report, because neither picture is wrong on its own.
    Theme ResolveDocumentTheme(_In_ LayoutDocument const& document) noexcept;
}
