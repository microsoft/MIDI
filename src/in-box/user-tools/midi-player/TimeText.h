// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <cstdint>
#include <cwctype>
#include <optional>
#include <string_view>

namespace midiplayer
{
    // Reads a time a person typed: "1:25", "01:25", "1:02:03", or a plain number of seconds such
    // as "85". Spaces around it are ignored. Anything else is not a time, and the caller says so
    // rather than guessing what was meant.
    //
    // Header only, so the unit tests read times exactly as the player does.
    inline std::optional<uint64_t> ParseTimeText(_In_ std::wstring_view text) noexcept
    {
        while (!text.empty() && std::iswspace(text.front()))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() && std::iswspace(text.back()))
        {
            text.remove_suffix(1);
        }

        // Nine digits in one part is over thirty years of seconds, and keeps every sum below far
        // from overflowing.
        constexpr size_t MaximumDigits = 9;

        uint64_t parts[3]{};
        size_t partCount = 0;
        size_t digits = 0;
        uint64_t current = 0;

        for (auto const character : text)
        {
            if (character >= L'0' && character <= L'9')
            {
                if (++digits > MaximumDigits)
                {
                    return std::nullopt;
                }

                current = current * 10 + static_cast<uint64_t>(character - L'0');
            }
            else if (character == L':')
            {
                if (digits == 0 || partCount == 2)
                {
                    return std::nullopt;
                }

                parts[partCount++] = current;
                current = 0;
                digits = 0;
            }
            else
            {
                return std::nullopt;
            }
        }

        // Empty, or ending in a colon.
        if (digits == 0)
        {
            return std::nullopt;
        }

        parts[partCount++] = current;

        switch (partCount)
        {
        case 1:
            return parts[0];

        case 2:
            if (parts[1] >= 60)
            {
                return std::nullopt;
            }

            return parts[0] * 60 + parts[1];

        case 3:
            if (parts[1] >= 60 || parts[2] >= 60)
            {
                return std::nullopt;
            }

            return parts[0] * 3600 + parts[1] * 60 + parts[2];

        default:
            return std::nullopt;
        }
    }
}
