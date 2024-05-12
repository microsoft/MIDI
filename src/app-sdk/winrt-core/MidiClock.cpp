// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiClock.h"
#include "MidiClock.g.cpp"



namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint64_t MidiClock::m_timestampFrequency{ 0 };

    internal::MidiTimestamp MidiClock::Now() 
    { 
        return internal::GetCurrentMidiTimestamp(); 
    }

    uint64_t MidiClock::TimestampFrequency() 
    { 
        // this is not supposed to change over time, so we cache it
        if (m_timestampFrequency == 0)
            m_timestampFrequency = internal::GetMidiTimestampFrequency();

        return m_timestampFrequency;
    }


    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampByTicks(
        internal::MidiTimestamp timestampValue, 
        int64_t offsetTicks)
    {
        // TODO: should also check for future wrap
        return timestampValue + offsetTicks < 0 ? 0 : timestampValue + offsetTicks;
    }


    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampByMicroseconds(
        internal::MidiTimestamp timestampValue, 
        int64_t offsetMicroseconds)
    {
        uint64_t offsetTicks;

        offsetTicks = (uint64_t)(offsetMicroseconds * TimestampFrequency() / (double)MICROSECONDS_PER_SECOND);

        return OffsetTimestampByTicks(timestampValue, offsetTicks);
    }


    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampByMilliseconds(
        internal::MidiTimestamp timestampValue, 
        int64_t offsetMilliseconds)
    {
        uint64_t offsetTicks;

        offsetTicks = (uint64_t)(offsetMilliseconds * TimestampFrequency() / (double)MILLISECONDS_PER_SECOND);

        return OffsetTimestampByTicks(timestampValue, offsetTicks);
    }

    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampBySeconds(
        internal::MidiTimestamp timestampValue,
        int64_t offsetSeconds)
    {
        uint64_t offsetTicks;

        offsetTicks = (uint64_t)(offsetSeconds * TimestampFrequency());

        return OffsetTimestampByTicks(timestampValue, offsetTicks);
    }



    _Use_decl_annotations_
        double MidiClock::ConvertTimestampTicksToNanoseconds(
            internal::MidiTimestamp const timestampValue)
    {

        return internal::ConvertTimestampToFractionalNanoseconds(timestampValue, TimestampFrequency());
    }

    _Use_decl_annotations_
    double MidiClock::ConvertTimestampTicksToMicroseconds(
        internal::MidiTimestamp const timestampValue)
    {

        return internal::ConvertTimestampToFractionalMicroseconds(timestampValue, TimestampFrequency());
    }


    _Use_decl_annotations_
    double MidiClock::ConvertTimestampTicksToMilliseconds(
        internal::MidiTimestamp const timestampValue)
    {
        return internal::ConvertTimestampToFractionalMilliseconds(timestampValue, TimestampFrequency());
    }

    _Use_decl_annotations_
    double MidiClock::ConvertTimestampTicksToSeconds(
        internal::MidiTimestamp const timestampValue)
    {
        return internal::ConvertTimestampToFractionalSeconds(timestampValue, TimestampFrequency());
    }

}
