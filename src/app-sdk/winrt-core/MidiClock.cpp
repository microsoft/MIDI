// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiClock.h"
#include "MidiClock.g.cpp"


#include <mmsystem.h>
#include <timeapi.h>
#include <winternl.h>

using namespace std::chrono_literals;

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
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



    midi2::MidiSystemTimerSettings MidiClock::GetCurrentSystemTimerInfo()
    {
        ULONG minResolution{ 0 };
        ULONG maxResolution{ 0 };
        ULONG curResolution{ 0 };

        auto queryResult = ::NtQueryTimerResolution(&maxResolution, &minResolution, &curResolution);

        midi2::MidiSystemTimerSettings timerSettings{};

        if (queryResult == STATUS_SUCCESS)
        {
            // we convert everything to ticks to make it more usable with the rest of MIDI

            auto minResolutionTicks = (uint64_t)(((uint64_t)minResolution * m_timerResolutionFrequency) / TimestampFrequency());
            auto maxResolutionTicks = (uint64_t)(((uint64_t)maxResolution * m_timerResolutionFrequency) / TimestampFrequency());
            auto curResolutionTicks = (uint64_t)(((uint64_t)curResolution * m_timerResolutionFrequency) / TimestampFrequency());

            timerSettings.MinimumIntervalTicks = minResolutionTicks;
            timerSettings.MaximumIntervalTicks = maxResolutionTicks;
            timerSettings.CurrentIntervalTicks = curResolutionTicks;
        }
        else
        {
            // blank struct is returned
        }

        return timerSettings;

    }

    //_Use_decl_annotations_
    //bool MidiClock::SetSystemTimerResolutionMilliseconds(uint32_t targetIntervalMilliseconds)
    //{
    //    auto timerInfo = GetCurrentSystemTimerInfo();

    //    if (targetIntervalMilliseconds > ConvertTimestampTicksToMilliseconds(timerInfo.MaximumIntervalTicks)) return false;
    //    if (targetIntervalMilliseconds < ConvertTimestampTicksToMilliseconds(timerInfo.MinimumIntervalTicks)) return false;

    //    //auto newTimerValue = targetIntervalTicks * TimestampFrequency() / m_timerResolutionFrequency;
    //    auto newTimerValue = ConvertTimestampTicksToMilliseconds(targetIntervalMilliseconds);

    //    // TODO: Call API to set the new timer value
    //    // NtSetTimerResolution exists but isn't coming up in the user-mode headers.
    //    // there are also the mmsystem API calls we can use
    //    
    //    auto configResult = timeBeginPeriod(targetIntervalMilliseconds);

    //    if (configResult == TIMERR_NOERROR)
    //    {
    //        return true;
    //    }
    //    else
    //    {
    //        return false;
    //    }

    //}




}
