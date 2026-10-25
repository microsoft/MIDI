// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A note number box that also takes a note's name. Typing "F#0", "Bb1" or "C3" sets 30, 35 or
// 60, the way every Windows MIDI Services tool names notes (middle C, 60, is C3). Shared with
// MIDI Sequencer's note boxes through NoteNames.h, so the two apps read a note alike.

#include <winrt/Windows.Globalization.NumberFormatting.h>

#include "NoteNames.h"
#include "PadGrid.h"

namespace winrt::midiglass::implementation
{
    struct NoteNumberFormatter : winrt::implements<
        NoteNumberFormatter,
        ::winrt::Windows::Globalization::NumberFormatting::INumberFormatter2,
        ::winrt::Windows::Globalization::NumberFormatting::INumberParser>
    {
        // With names shown, a box reads "F♯0 (30)". Without, it reads "30" and still takes a name,
        // for a box that already has the name printed beside it.
        explicit NoteNumberFormatter(bool showsName) noexcept : m_showsName(showsName)
        {
        }

        winrt::hstring FormatInt(int64_t value) const { return FormatDouble(static_cast<double>(value)); }
        winrt::hstring FormatUInt(uint64_t value) const { return FormatDouble(static_cast<double>(value)); }

        winrt::hstring FormatDouble(double value) const
        {
            auto const note = std::isfinite(value) ? static_cast<int32_t>(std::clamp(std::lround(value), 0L, 127L)) : 0;

            if (!m_showsName)
            {
                return winrt::hstring{ std::to_wstring(note) };
            }

            return winrt::hstring{ ::glass::PadNoteName(note, false) + L" (" + std::to_wstring(note) + L")" };
        }

        foundation::IReference<int64_t> ParseInt(winrt::hstring const& text) const
        {
            auto const note = ::midiapp::ParseNoteNumber(text);
            return note.has_value() ? foundation::IReference<int64_t>{ static_cast<int64_t>(*note) } : nullptr;
        }

        foundation::IReference<uint64_t> ParseUInt(winrt::hstring const& text) const
        {
            auto const note = ::midiapp::ParseNoteNumber(text);
            return note.has_value() ? foundation::IReference<uint64_t>{ static_cast<uint64_t>(*note) } : nullptr;
        }

        foundation::IReference<double> ParseDouble(winrt::hstring const& text) const
        {
            auto const note = ::midiapp::ParseNoteNumber(text);
            return note.has_value() ? foundation::IReference<double>{ static_cast<double>(*note) } : nullptr;
        }

    private:
        bool m_showsName{ true };
    };

    inline void UseNoteNames(_In_ controls::NumberBox const& box, _In_ bool showsName)
    {
        box.NumberFormatter(winrt::make<NoteNumberFormatter>(showsName));
    }
}
