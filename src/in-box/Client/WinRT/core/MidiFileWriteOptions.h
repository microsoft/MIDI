// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiFileWriteOptions.g.h"

#include "midi_file_smf_writer.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiFileWriteOptions : MidiFileWriteOptionsT<MidiFileWriteOptions>
    {
        MidiFileWriteOptions() = default;

        bool WriteSingleTrack() const noexcept { return m_options.SingleTrack; }
        void WriteSingleTrack(_In_ bool const value) noexcept { m_options.SingleTrack = value; }

        bool UseRunningStatus() const noexcept { return m_options.UseRunningStatus; }
        void UseRunningStatus(_In_ bool const value) noexcept { m_options.UseRunningStatus = value; }

        uint16_t AbsoluteTimingTicksPerQuarterNote() const noexcept { return m_options.AbsoluteTimingTicksPerQuarterNote; }
        void AbsoluteTimingTicksPerQuarterNote(_In_ uint16_t const value) noexcept { m_options.AbsoluteTimingTicksPerQuarterNote = value; }

        uint64_t MaximumFileBytes() const noexcept { return m_options.MaximumFileBytes; }
        void MaximumFileBytes(_In_ uint64_t const value) noexcept { m_options.MaximumFileBytes = value; }

        ::midifile::WriteOptions const& InternalOptions() const noexcept { return m_options; }

    private:
        ::midifile::WriteOptions m_options{};
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Files::factory_implementation
{
    struct MidiFileWriteOptions : MidiFileWriteOptionsT<MidiFileWriteOptions, implementation::MidiFileWriteOptions>
    {
    };
}
