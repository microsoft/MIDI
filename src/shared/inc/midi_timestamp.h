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

		KeQueryPerformanceCounter(&frequency);

		return frequency.QuadPart;
	}
#else
	std::uint64_t GetCurrentMidiTimestamp()
	{
		LARGE_INTEGER timestamp;

		QueryPerformanceCounter(&timestamp);

		return timestamp.QuadPart;
	}


	std::uint64_t GetMidiTimestampFrequency()
	{
		LARGE_INTEGER frequency;

		QueryPerformanceCounter(&frequency);

		return frequency.QuadPart;
	}
#endif

}
