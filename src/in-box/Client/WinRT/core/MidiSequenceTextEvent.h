// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequenceTextEvent.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiSequenceTextEvent : MidiSequenceTextEventT<MidiSequenceTextEvent>
    {
        MidiSequenceTextEvent() = default;

        uint32_t Tick() const noexcept { return m_tick; }
        uint16_t TrackIndex() const noexcept { return m_trackIndex; }
        Sequencing::MidiSequenceTextKind Kind() const noexcept { return m_kind; }
        winrt::hstring Text() const noexcept { return m_text; }

        void InternalInitialize(
            _In_ uint32_t const tick,
            _In_ uint16_t const trackIndex,
            _In_ Sequencing::MidiSequenceTextKind const kind,
            _In_ winrt::hstring const& text) noexcept;

    private:
        uint32_t m_tick{ 0 };
        uint16_t m_trackIndex{ 0 };
        Sequencing::MidiSequenceTextKind m_kind{ Sequencing::MidiSequenceTextKind::Text };
        winrt::hstring m_text{};
    };
}
