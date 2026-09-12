// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
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

namespace winrt::Windows::Devices::Midi2::implementation
{
    UINT MidiClock::m_lastTimeBeginPeriodValue{};
    uint32_t MidiClock::m_lowLatencyPeriodRefCount{ 0 };
    std::mutex MidiClock::m_lowLatencyPeriodLock{};
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
        if (offsetTicks >= 0)
        {
            return timestampValue + static_cast<uint64_t>(offsetTicks);
        }

        // Magnitude taken in unsigned arithmetic so INT64_MIN stays defined.
        auto const magnitude = 0ull - static_cast<uint64_t>(offsetTicks);

        return timestampValue > magnitude ? timestampValue - magnitude : 0;
    }


    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampByMicroseconds(
        internal::MidiTimestamp timestampValue, 
        int64_t offsetMicroseconds)
    {
        // Signed integer arithmetic throughout. Multiplying by the unsigned frequency first, as this
        // used to, converts a negative offset into a huge positive one.
        auto const offsetTicks =
            (offsetMicroseconds * static_cast<int64_t>(TimestampFrequency())) / MICROSECONDS_PER_SECOND;

        return OffsetTimestampByTicks(timestampValue, offsetTicks);
    }


    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampByMilliseconds(
        internal::MidiTimestamp timestampValue, 
        int64_t offsetMilliseconds)
    {
        auto const offsetTicks =
            (offsetMilliseconds * static_cast<int64_t>(TimestampFrequency())) / MILLISECONDS_PER_SECOND;

        return OffsetTimestampByTicks(timestampValue, offsetTicks);
    }

    _Use_decl_annotations_
    internal::MidiTimestamp MidiClock::OffsetTimestampBySeconds(
        internal::MidiTimestamp timestampValue,
        int64_t offsetSeconds)
    {
        auto const offsetTicks = offsetSeconds * static_cast<int64_t>(TimestampFrequency());

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
            // The values arrive in fixed 100ns units, so scaling to clock ticks multiplies by the
            // clock frequency. Doing it the other way round only happens to work at 10 MHz.
            auto minResolutionTicks = (uint64_t)(((uint64_t)minResolution * TimestampFrequency()) / m_timerResolutionFrequency);
            auto maxResolutionTicks = (uint64_t)(((uint64_t)maxResolution * TimestampFrequency()) / m_timerResolutionFrequency);
            auto curResolutionTicks = (uint64_t)(((uint64_t)curResolution * TimestampFrequency()) / m_timerResolutionFrequency);

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

    // A good article about how these functions work post-2020.
    // https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/

    bool MidiClock::BeginLowLatencySystemTimerPeriod()
    {
        std::scoped_lock<std::mutex> lock(m_lowLatencyPeriodLock);

        // Already held by someone else in this process, so the caller is covered either way.
        if (m_lowLatencyPeriodRefCount > 0)
        {
            m_lowLatencyPeriodRefCount++;
            return true;
        }

        TIMECAPS caps{};

        auto timecapsResult = timeGetDevCaps(&caps, sizeof(caps));

        if (timecapsResult != MMSYSERR_NOERROR) return false;

        auto result = timeBeginPeriod(caps.wPeriodMin);

        if (result != MMSYSERR_NOERROR) return false;

        m_lastTimeBeginPeriodValue = caps.wPeriodMin;
        m_lowLatencyPeriodRefCount = 1;

        return true;
    }

    bool MidiClock::EndLowLatencySystemTimerPeriod()
    {
        std::scoped_lock<std::mutex> lock(m_lowLatencyPeriodLock);

        if (m_lowLatencyPeriodRefCount == 0) return false;

        m_lowLatencyPeriodRefCount--;

        // Someone else in this process still wants it.
        if (m_lowLatencyPeriodRefCount > 0) return true;

        auto result = timeEndPeriod(m_lastTimeBeginPeriodValue);

        m_lastTimeBeginPeriodValue = 0;

        return result == MMSYSERR_NOERROR;
    }


}
