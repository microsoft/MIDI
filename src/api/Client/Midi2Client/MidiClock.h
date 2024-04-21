// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiClock.g.h"

//#include <midi_timestamp.h>

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiClock : MidiClockT<MidiClock>
    {
        MidiClock() = default;

        static internal::MidiTimestamp Now();

        static internal::MidiTimestamp TimestampConstantSendImmediately() { return MIDI_TIMESTAMP_SEND_IMMEDIATELY; }

        static uint64_t TimestampFrequency();

        static internal::MidiTimestamp OffsetTimestampByTicks(
            _In_ internal::MidiTimestamp const timestampValue, 
            _In_ int64_t const offsetTicks);

        static internal::MidiTimestamp OffsetTimestampByMicroseconds(
            _In_ internal::MidiTimestamp const timestampValue, 
            _In_ int64_t const offsetMicroseconds);

        static internal::MidiTimestamp OffsetTimestampByMilliseconds(
            _In_ internal::MidiTimestamp const timestampValue, 
            _In_ int64_t const offsetMilliseconds);

        static internal::MidiTimestamp OffsetTimestampBySeconds(
            _In_ internal::MidiTimestamp const timestampValue,
            _In_ int64_t const offsetSeconds);

        static double ConvertTimestampTicksToNanoseconds(_In_ internal::MidiTimestamp const timestampValue);
        static double ConvertTimestampTicksToMicroseconds(_In_ internal::MidiTimestamp const timestampValue);
        static double ConvertTimestampTicksToMilliseconds(_In_ internal::MidiTimestamp const timestampValue);
        static double ConvertTimestampTicksToSeconds(_In_ internal::MidiTimestamp const timestampValue);

    private:
        static uint64_t m_timestampFrequency;
    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiClock : MidiClockT<MidiClock, implementation::MidiClock, winrt::static_lifetime>
    {
    };
}
