// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure: no precompiled header and no WinRT, so document code and unit tests can use it.

#include <string_view>

namespace midiapp
{
    // What text is drawn in when it names no font, and after the named one on a PC without it.
    constexpr wchar_t DefaultFontFamily[] = L"Segoe UI Variable Text";

    // Whether a family name read from a file can be handed to XAML as it is.
    inline bool IsSafeFontFamilyName(std::wstring_view name) noexcept
    {
        // Longer than any family name on a PC, and short enough that nobody can use it to carry
        // anything else.
        constexpr size_t MaximumFontFamilyLength = 128;

        if (name.empty() || name.size() > MaximumFontFamilyLength)
        {
            return false;
        }

        // A path, a link or a font file is written with these, and a comma makes a list of
        // families. None of them is part of a family's own name.
        for (auto const ch : name)
        {
            if (ch < L' ' || ch == L'\\' || ch == L'/' || ch == L':' || ch == L'#' || ch == L',' ||
                ch == L'%' || ch == L'?' || ch == L'*' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
            {
                return false;
            }
        }

        return name.find_first_not_of(L' ') != std::wstring_view::npos;
    }
}
