// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <algorithm>

//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;


TEST_CASE("Offline.Clock.Timestamp Clock Basics")
{
    SECTION("MIDI timestamp appears to work")
    {
        for (int i = 0; i < 20; i++)
        {
            std::cout << "Timestamp: " << i << " : " << MidiClock::GetMidiTimestamp() << std::endl;
        }
    }

    SECTION("MIDI frequency appears to work")
    {
        std::cout << "Timestamp Frequency: " << MidiClock::GetMidiTimestampFrequency() << std::endl;
    }

}



TEST_CASE("Offline.Clock.Timestamp Clock Benchmark")
{
    SECTION("Benchmark clock call")
    {
        // just loops for Frequency times

        auto freq = MidiClock::GetMidiTimestampFrequency();
        auto loopCount = freq;

        REQUIRE(loopCount > 0);

        auto startTime = MidiClock::GetMidiTimestamp();

        uint64_t fooAccumulator{0};
        uint64_t foo;

        for (int i = 0; i < loopCount; i++)
        {
            // this could get optimized out in a release compile, so we use it, knowing that adds a tiny bit of overhead
            foo = MidiClock::GetMidiTimestamp();
            fooAccumulator |= foo;
        }

        std::cout << "(Foo accumulator - just to prevent optimization: " << fooAccumulator << ")" << std::endl;

        auto endTime = MidiClock::GetMidiTimestamp();

        auto averageTicks = (endTime - startTime) / (double)loopCount;
        auto averageSeconds = averageTicks / (double)freq;
        auto averageMilliseconds = averageSeconds * 1000;
        auto averageMicroseconds = averageMilliseconds * 1000;

        std::cout << "Loop count             : " << std::dec << loopCount << std::endl;
        std::cout << "Average Ticks per loop : " << std::dec << std::fixed << averageTicks << " ticks,  " << averageMilliseconds << "ms, " << averageMicroseconds << " microseconds." << std::endl;

    }

}
