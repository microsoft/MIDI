// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequenceTrack.g.h"

#include "midi_file_sequence.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiSequenceTrack : MidiSequenceTrackT<MidiSequenceTrack>
    {
        MidiSequenceTrack() = default;

        uint16_t TrackIndex() const noexcept { return m_trackIndex; }

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring InstrumentName() const noexcept { return m_instrumentName; }
        winrt::hstring SuggestedDeviceName() const noexcept { return m_suggestedDeviceName; }

        uint16_t UsedChannelMask() const noexcept { return m_usedChannelMask; }

        uint32_t NoteCount() const noexcept { return m_noteCount; }
        uint32_t EventCount() const noexcept { return m_eventCount; }
        uint32_t LastTick() const noexcept { return m_lastTick; }

        void InternalInitialize(_In_ uint16_t const trackIndex, _In_ ::midifile::Track const& track);

    private:
        uint16_t m_trackIndex{ 0 };
        winrt::hstring m_name{};
        winrt::hstring m_instrumentName{};
        winrt::hstring m_suggestedDeviceName{};
        uint16_t m_usedChannelMask{ 0 };
        uint32_t m_noteCount{ 0 };
        uint32_t m_eventCount{ 0 };
        uint32_t m_lastTick{ 0 };
    };
}
