// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The controls on each MIDI Mixer strip: the controllers General MIDI synths and most others
// listen to, and where a General MIDI synth starts each one. Include after StringResources.h.

namespace winrt::midisequencer::implementation::mixer
{
    struct MixerSlot
    {
        uint8_t Controller{ 0 };
        wchar_t const* NameKey{ nullptr };
        wchar_t const* ShortKey{ nullptr };

        // Where a General MIDI synth starts, shown until the track sets it, and where a
        // double-click puts the control back.
        uint8_t Standard{ 0 };
    };

    inline constexpr MixerSlot Volume{ 7, L"MixerVolume", L"MixerVolume", 100 };
    inline constexpr MixerSlot Pan{ 10, L"MixerPan", L"MixerPanShort", 64 };
    inline constexpr MixerSlot Expression{ 11, L"MixerExpression", L"MixerExpressionShort", 127 };
    inline constexpr MixerSlot Reverb{ 91, L"MixerReverb", L"MixerReverbShort", 40 };
    inline constexpr MixerSlot Chorus{ 93, L"MixerChorus", L"MixerChorusShort", 0 };

    // The small knobs along the top of a strip.
    inline constexpr std::array<MixerSlot, 3> Knobs{ Expression, Reverb, Chorus };

    inline MixerSlot const& SlotFor(uint8_t const controller) noexcept
    {
        for (auto const& slot : Knobs)
        {
            if (slot.Controller == controller)
            {
                return slot;
            }
        }

        return controller == Pan.Controller ? Pan : Volume;
    }

    // A control's position is the MIDI 1.0 value, 0 to 127. What's sent and kept is 32 bits.
    inline uint32_t ToValue(double const position) noexcept
    {
        return ::midisequencer::ScaleUp(static_cast<uint32_t>(std::clamp(std::lround(position), 0L, 127L)), 7, 32);
    }

    inline double ToPosition(uint32_t const value) noexcept
    {
        return static_cast<double>(::midisequencer::ScaleDown(value, 32, 7));
    }

    // Pan reads as left, center or right of the middle: L24, C, R18. The rest are their numbers.
    inline std::wstring ValueText(uint8_t const controller, double const position)
    {
        auto const value = static_cast<int32_t>(std::lround(position));

        if (controller != Pan.Controller)
        {
            return std::to_wstring(value);
        }

        if (value == 64)
        {
            return std::wstring{ ::midisequencer::resources::GetString(L"MixerPanCenter") };
        }

        return std::wstring{ ::midisequencer::resources::FormatString(value < 64 ? L"MixerPanLeftFormat" : L"MixerPanRightFormat", std::abs(value - 64)) };
    }

    // The same, as a screen reader says it: "left 24", "center", "right 18".
    inline std::wstring SpokenPan(double const position)
    {
        auto const value = static_cast<int32_t>(std::lround(position));

        if (value == 64)
        {
            return std::wstring{ ::midisequencer::resources::GetString(L"MixerPanCenterSpoken") };
        }

        return std::wstring{ ::midisequencer::resources::FormatString(value < 64 ? L"MixerPanLeftSpokenFormat" : L"MixerPanRightSpokenFormat", std::abs(value - 64)) };
    }
}
