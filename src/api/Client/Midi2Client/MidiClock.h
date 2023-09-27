// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiClock.g.h"

#include <midi_timestamp.h>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiClock : MidiClockT<MidiClock>
    {
        MidiClock() = default;

        static internal::MidiTimestamp GetMidiTimestamp() { return ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp(); }
        static internal::MidiTimestamp GetMidiTimestampFrequency() { return ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency(); }

        static internal::MidiTimestamp ConvertTimestampToMicroseconds(_In_ internal::MidiTimestamp const timestampValue)
        {
            auto freq = GetMidiTimestampFrequency();

            return (internal::MidiTimestamp)((timestampValue * 1000000) / freq);
        }
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiClock : MidiClockT<MidiClock, implementation::MidiClock>
    {
    };
}
