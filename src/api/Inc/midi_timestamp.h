// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_TIMESTAMP_H
#define MIDI_TIMESTAMP_H


#include <Windows.h>
#include <profileapi.h>

// Info on high resolution counters/timestamps in Windows
// https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps


#define NANOSECONDS_PER_SECOND      1000000000
#define MICROSECONDS_PER_SECOND     1000000
#define MILLISECONDS_PER_SECOND     1000

namespace WindowsMidiServicesInternal
{

#ifdef _KERNEL_MODE
	std::uint64_t GetCurrentMidiTimestamp()
	{
		LARGE_INTEGER timestamp;

		timestamp = KeQueryPerformanceCounter(NULL);

		return timestamp.QuadPart;
	}


	std::uint64_t GetMidiTimestampFrequency()
	{
		LARGE_INTEGER frequency;

		KeQueryPerformanceFrequency(&frequency);

		return frequency.QuadPart;
	}
#else
	inline std::uint64_t GetCurrentMidiTimestamp()
	{
		LARGE_INTEGER timestamp;

		QueryPerformanceCounter(&timestamp);

		return timestamp.QuadPart;
	}


	inline std::uint64_t GetMidiTimestampFrequency()
	{
		LARGE_INTEGER frequency;

		QueryPerformanceFrequency(&frequency);

		return frequency.QuadPart;
	}
#endif




    // truncates, not rounds
    inline uint64_t ConvertTimestampToWholeMilliseconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (uint64_t)((timestampValue * (uint64_t)MILLISECONDS_PER_SECOND) / timestampFrequency);
    }

    // truncates, not rounds
    inline uint64_t ConvertTimestampToWholeMicroseconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (uint64_t)((timestampValue * (uint64_t)MICROSECONDS_PER_SECOND) / timestampFrequency);
    }



    inline double ConvertTimestampToFractionalSeconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (double)((double)timestampValue / (double)timestampFrequency);
    }

    inline double ConvertTimestampToFractionalMilliseconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (double)((timestampValue * (double)MILLISECONDS_PER_SECOND) / timestampFrequency);
    }

    inline double ConvertTimestampToFractionalMicroseconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (double)((timestampValue * (double)MICROSECONDS_PER_SECOND) / timestampFrequency);
    }

    inline double ConvertTimestampToFractionalNanoseconds(_In_ uint64_t const timestampValue, _In_ uint64_t timestampFrequency)
    {
        return (double)((timestampValue * (double)NANOSECONDS_PER_SECOND) / timestampFrequency);
    }

    
	// TODO: Consider adding in GetSystemTimePreciseAsFileTime for jitter measurements (KeQuerySystemTimePrecise for driver code)
	// https://learn.microsoft.com/windows/win32/sysinfo/acquiring-high-resolution-time-stamps



}


#endif