// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <Windows.h>
#include <profileapi.h>

// Info on high resolution counters/timestamps in Windows
// https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps

namespace Windows::Devices::Midi2::Internal::Shared
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


	// TODO: Consider adding in GetSystemTimePreciseAsFileTime for jitter measurements (KeQuerySystemTimePrecise for driver code)
	// https://learn.microsoft.com/windows/win32/sysinfo/acquiring-high-resolution-time-stamps

#endif

}
