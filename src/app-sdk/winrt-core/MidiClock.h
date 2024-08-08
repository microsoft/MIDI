// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiClock.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
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


        static midi2::MidiSystemTimerSettings GetCurrentSystemTimerInfo();
    //    static bool SetSystemTimerResolutionMilliseconds(_In_ uint32_t targetIntervalMilliseconds);

    private:
        static uint64_t m_timestampFrequency;

        static const uint64_t m_timerResolutionFrequency{ 10'000'000 };    // 100ns fixed units for timer API calls we use


    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiClock : MidiClockT<MidiClock, implementation::MidiClock, winrt::static_lifetime>
    {
    };
}
