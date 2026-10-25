// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A MIDI note typed the way a person writes it: "52", "E2", "C#3", "Bb1", "F♯0", "B♭-1", or a box's
// own "E2 (52)". Header only and plain C++ with no WinRT, so every tool and the unit tests read
// notes the same way.
//
// Middle C, note 60, is C3, the way every Windows MIDI Services tool names notes.

#include <sal.h>

#include <cstdint>
#include <cwctype>
#include <optional>
#include <string_view>

namespace midiapp
{
    namespace notenames
    {
        constexpr wchar_t Sharp = L'\u266F';
        constexpr wchar_t Flat = L'\u266D';

        inline std::wstring_view Trim(_In_ std::wstring_view text) noexcept
        {
            auto const first = text.find_first_not_of(L" \t");

            if (first == std::wstring_view::npos)
            {
                return {};
            }

            return text.substr(first, text.find_last_not_of(L" \t") - first + 1);
        }

        // Digits, with an optional minus sign in front. Nothing when anything else is there, or
        // when there are no digits at all.
        inline std::optional<int32_t> ReadWhole(_In_ std::wstring_view text) noexcept
        {
            text = Trim(text);

            auto negative = false;

            if (!text.empty() && (text.front() == L'-' || text.front() == L'\u2212'))
            {
                negative = true;
                text.remove_prefix(1);
            }

            if (text.empty() || text.size() > 4)
            {
                return std::nullopt;
            }

            int32_t value{ 0 };

            for (auto const c : text)
            {
                if (c < L'0' || c > L'9')
                {
                    return std::nullopt;
                }

                value = value * 10 + (c - L'0');
            }

            return negative ? -value : value;
        }
    }

    // The note number, 0 to 127, or nothing when the text is not a note or names one outside
    // that range. A letter A to G in either case, then sharps (# or ♯) or flats (b or ♭), up to
    // two, then the octave, which can be below zero. A number in brackets after a name wins,
    // because that is what a box that shows "E2 (52)" gives back when it is left alone.
    inline std::optional<int32_t> ParseNoteNumber(_In_ std::wstring_view text) noexcept
    {
        text = notenames::Trim(text);

        if (text.empty())
        {
            return std::nullopt;
        }

        if (auto const open = text.find(L'('); open != std::wstring_view::npos)
        {
            auto const close = text.find(L')', open);
            text = text.substr(open + 1, close == std::wstring_view::npos ? std::wstring_view::npos : close - open - 1);
        }

        text = notenames::Trim(text);

        if (text.empty())
        {
            return std::nullopt;
        }

        std::optional<int32_t> number{};

        if (std::iswdigit(text.front()))
        {
            number = notenames::ReadWhole(text);
        }
        else
        {
            static constexpr int32_t Pitches[7]{ 9, 11, 0, 2, 4, 5, 7 };

            auto const letter = static_cast<wchar_t>(std::towupper(text.front()));

            if (letter < L'A' || letter > L'G')
            {
                return std::nullopt;
            }

            auto pitch = Pitches[letter - L'A'];
            size_t at{ 1 };

            while (at < text.size() && at < 3)
            {
                auto const c = text[at];

                if (c == L'#' || c == notenames::Sharp)
                {
                    ++pitch;
                }
                else if (c == L'b' || c == notenames::Flat)
                {
                    --pitch;
                }
                else
                {
                    break;
                }

                ++at;
            }

            auto const octave = notenames::ReadWhole(text.substr(at));

            if (!octave.has_value())
            {
                return std::nullopt;
            }

            number = (*octave + 2) * 12 + pitch;
        }

        if (!number.has_value() || *number < 0 || *number > 127)
        {
            return std::nullopt;
        }

        return number;
    }
}
