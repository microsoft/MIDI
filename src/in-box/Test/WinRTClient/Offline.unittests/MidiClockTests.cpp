// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"



void MidiClockTests::TestMidiClockBasics()
{
    for (int i = 0; i < 10; i++)
    {
        std::cout << "Timestamp: " << i << " : " << MidiClock::Now() << std::endl;
    }

    VERIFY_IS_GREATER_THAN(MidiClock::TimestampFrequency(), (uint32_t)0);
}

// TODO: Should test the convenience methods as well

void MidiClockTests::TestTimestampOffsets()
{
    auto const frequency = MidiClock::TimestampFrequency();

    // Far enough from zero that the negative cases below do not clamp.
    uint64_t const base = frequency * 1000;

    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByTicks(base, 500), base + 500);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampBySeconds(base, 2), base + (frequency * 2));
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMilliseconds(base, 250), base + ((250 * frequency) / 1000));
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMicroseconds(base, 1000), base + ((1000 * frequency) / 1000000));

    // Negative offsets. The microsecond and millisecond forms used to multiply by the unsigned
    // frequency before dividing, which turned a negative offset into a huge positive one.
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByTicks(base, -500), base - 500);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampBySeconds(base, -2), base - (frequency * 2));
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMilliseconds(base, -250), base - ((250 * frequency) / 1000));
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMicroseconds(base, -1000), base - ((1000 * frequency) / 1000000));

    // Going below zero clamps instead of wrapping to a huge timestamp.
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByTicks(100, -500), (uint64_t)0);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampBySeconds(100, -5), (uint64_t)0);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMilliseconds(100, -5000), (uint64_t)0);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMicroseconds(100, -5000000), (uint64_t)0);

    // Subtracting a device latency and adding it back is the case this exists for.
    auto const compensated = MidiClock::OffsetTimestampByMilliseconds(base, -5);
    VERIFY_ARE_EQUAL(MidiClock::OffsetTimestampByMilliseconds(compensated, 5), base);
}

void MidiClockTests::TestLowLatencyPeriodRefCounting()
{
    // Two independent components in one process, which is what this SDK sees inside a DAW hosting
    // plugins. The second caller must be told it is covered, not refused.
    VERIFY_IS_TRUE(MidiClock::BeginLowLatencySystemTimerPeriod());
    VERIFY_IS_TRUE(MidiClock::BeginLowLatencySystemTimerPeriod());

    // The first release must not drop the period out from under the second caller.
    VERIFY_IS_TRUE(MidiClock::EndLowLatencySystemTimerPeriod());
    VERIFY_IS_TRUE(MidiClock::EndLowLatencySystemTimerPeriod());

    // Balanced now, so there is nothing left to release.
    VERIFY_IS_FALSE(MidiClock::EndLowLatencySystemTimerPeriod());
}