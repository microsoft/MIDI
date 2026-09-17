// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiFileReadOptions.g.h"

#include "midi_file_smf_reader.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiFileReadOptions : MidiFileReadOptionsT<MidiFileReadOptions>
    {
        MidiFileReadOptions() = default;

        uint64_t MaximumFileBytes() const noexcept { return m_limits.MaximumFileBytes; }
        void MaximumFileBytes(_In_ uint64_t const value) noexcept { m_limits.MaximumFileBytes = value; }

        uint32_t MaximumTrackCount() const noexcept { return m_limits.MaximumTracks; }
        void MaximumTrackCount(_In_ uint32_t const value) noexcept { m_limits.MaximumTracks = value; }

        uint32_t MaximumEventCount() const noexcept { return m_limits.MaximumEvents; }
        void MaximumEventCount(_In_ uint32_t const value) noexcept { m_limits.MaximumEvents = value; }

        uint32_t MaximumTextEventCount() const noexcept { return m_limits.MaximumTextEvents; }
        void MaximumTextEventCount(_In_ uint32_t const value) noexcept { m_limits.MaximumTextEvents = value; }

        uint32_t MaximumSingleMessageBytes() const noexcept { return m_limits.MaximumSingleMessageBytes; }
        void MaximumSingleMessageBytes(_In_ uint32_t const value) noexcept { m_limits.MaximumSingleMessageBytes = value; }

        bool FailOnUnreadableData() const noexcept { return m_failOnUnreadableData; }
        void FailOnUnreadableData(_In_ bool const value) noexcept { m_failOnUnreadableData = value; }

        ::midifile::ReadLimits const& InternalLimits() const noexcept { return m_limits; }

    private:
        ::midifile::ReadLimits m_limits{};
        bool m_failOnUnreadableData{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Files::factory_implementation
{
    struct MidiFileReadOptions : MidiFileReadOptionsT<MidiFileReadOptions, implementation::MidiFileReadOptions>
    {
    };
}
