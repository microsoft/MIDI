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

        static internal::MidiTimestamp Now() { return ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp(); }

        static uint64_t GetMidiTimestampFrequency() { return ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency(); }


        static double ConvertTimestampToMicroseconds(
            _In_ internal::MidiTimestamp const timestampValue)
        {
            auto freq = GetMidiTimestampFrequency();

            return (double)((timestampValue * (double)1000000.0) / freq);
        }

        static double ConvertTimestampToMilliseconds(
            _In_ internal::MidiTimestamp const timestampValue)
        {
            auto freq = GetMidiTimestampFrequency();

            return (double)((timestampValue * (double)1000.0) / freq);
        }
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiClock : MidiClockT<MidiClock, implementation::MidiClock, winrt::static_lifetime>
    {
    };
}
