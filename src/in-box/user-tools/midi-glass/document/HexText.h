// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.
//
// Hexadecimal in and out. A system exclusive dump travels as hex rather than an array of numbers
// because a firmware image is tens of thousands of bytes, and a person typing one into the editor
// wants to paste what the manual printed, spaces and line breaks and all.

#include <sal.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace glass
{
    // Upper case, no separators. This is the form that goes in the file.
    std::wstring ToHexBytes(_In_ std::vector<uint8_t> const& bytes) noexcept;

    // The same bytes with a space every byte and a line break every sixteen, which is what makes
    // a dump readable in a text box.
    std::wstring FormatHexBytes(_In_ std::vector<uint8_t> const& bytes) noexcept;

    // Whitespace, commas and an optional 0x on each byte are all accepted, because a person
    // pastes what the manual printed rather than what this app would have written.
    //
    // An odd number of digits, a character that is not hexadecimal, or more than the limit
    // returns nothing at all. A partly-read dump is worse than none: one bad character in a
    // firmware image can leave a synthesizer unusable.
    std::vector<uint8_t> ParseHexBytes(_In_ std::wstring_view text, _In_ size_t maximumBytes) noexcept;

    std::wstring FormatHexWords(_In_ std::vector<uint32_t> const& words) noexcept;

    // Up to maximumWords 32 bit words. Anything unreadable gives nothing back.
    std::vector<uint32_t> ParseHexWords(_In_ std::wstring_view text, _In_ size_t maximumWords) noexcept;
}
