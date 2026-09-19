// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequenceLyricLine.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiSequenceLyricLine : MidiSequenceLyricLineT<MidiSequenceLyricLine>
    {
        MidiSequenceLyricLine() = default;

        uint32_t StartTick() const noexcept { return m_startTick; }
        uint32_t EndTick() const noexcept { return m_endTick; }
        winrt::hstring Text() const noexcept { return m_text; }

        void InternalInitialize(
            _In_ uint32_t const startTick,
            _In_ uint32_t const endTick,
            _In_ winrt::hstring const& text) noexcept;

    private:
        uint32_t m_startTick{ 0 };
        uint32_t m_endTick{ 0 };
        winrt::hstring m_text{};
    };
}
