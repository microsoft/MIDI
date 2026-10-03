// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSendPacerTests.h"

#include "midi_send_pacer.h"

using namespace WindowsMidiServicesInternal;

#define CHECK_EQ(actual, expected) \
    do { auto const a_ = (actual); auto const e_ = (expected); VERIFY_IS_TRUE(a_ == e_, WEX::Common::String().Format(L"%hs == %hs, got %lld expected %lld", #actual, #expected, static_cast<long long>(a_), static_cast<long long>(e_))); } while (0)

namespace
{
    // 10 MHz, so a byte at wire speed is exactly 3,200 ticks
    constexpr uint64_t TicksPerSecond{ 10'000'000 };

    // well away from zero, as a real clock is
    constexpr uint64_t Start{ 100 * TicksPerSecond };

    // Sends messages of one size as soon as the pacer allows each, starting at Start. Returns
    // when the last one went.
    uint64_t SendBurst(_Inout_ MidiSendPacer& pacer, _In_ uint32_t const messageCount, _In_ uint32_t const messageBytes)
    {
        uint64_t now{ Start };

        for (uint32_t i = 0; i < messageCount; i++)
        {
            now += pacer.TicksUntilAllowed(messageBytes, now);

            VERIFY_ARE_EQUAL(pacer.TicksUntilAllowed(messageBytes, now), 0ull, L"a message is allowed once its wait is over");

            pacer.Charge(messageBytes, now);
        }

        return now;
    }
}


void MidiSendPacerTests::TestNoLimitNeverWaits()
{
    MidiSendPacer pacer;
    pacer.Configure(0, TicksPerSecond);

    VERIFY_IS_FALSE(pacer.IsLimited());

    for (int i = 0; i < 1000; i++)
    {
        CHECK_EQ(pacer.TicksUntilAllowed(1'000'000, Start), 0ull);
        pacer.Charge(1'000'000, Start);
    }
}

void MidiSendPacerTests::TestQuietConnectionIsNeverDelayed()
{
    MidiSendPacer pacer;
    pacer.Configure(1, TicksPerSecond);

    VERIFY_IS_TRUE(pacer.IsLimited());

    // one key a second, Note On and Note Off, for a minute
    for (uint64_t second = 0; second < 60; second++)
    {
        auto const now = Start + second * TicksPerSecond;

        CHECK_EQ(pacer.TicksUntilAllowed(3, now), 0ull);
        pacer.Charge(3, now);

        CHECK_EQ(pacer.TicksUntilAllowed(3, now + TicksPerSecond / 4), 0ull);
        pacer.Charge(3, now + TicksPerSecond / 4);
    }
}

void MidiSendPacerTests::TestChordGoesAtOnce()
{
    MidiSendPacer pacer;
    pacer.Configure(1, TicksPerSecond);

    // ten notes, all at the same moment
    for (int note = 0; note < 10; note++)
    {
        CHECK_EQ(pacer.TicksUntilAllowed(3, Start), 0ull);
        pacer.Charge(3, Start);
    }

    // and the same chord as one datagram
    MidiSendPacer together;
    together.Configure(1, TicksPerSecond);

    CHECK_EQ(together.TicksUntilAllowed(30, Start), 0ull);
}

void MidiSendPacerTests::TestBurstGoesAtWireSpeed()
{
    MidiSendPacer pacer;
    pacer.Configure(1, TicksPerSecond);

    // 3,000 bytes in 8-byte messages. The first 64 go at once, then one message every 2.56 ms.
    auto const elapsed = SendBurst(pacer, 375, 8) - Start;

    WEX::Logging::Log::Comment(WEX::Common::String().Format(L"3,000 bytes took %llu ticks at wire speed", elapsed));

    CHECK_EQ(elapsed, 367ull * 25'600ull);

    // a message sent right after the burst waits for it, as on a MIDI 1.0 cable
    auto const end = Start + elapsed;
    VERIFY_IS_GREATER_THAN(pacer.TicksUntilAllowed(3, end), 0ull);
}

void MidiSendPacerTests::TestBurstGoesFasterAtAHigherLimit()
{
    MidiSendPacer pacer;
    pacer.Configure(32, TicksPerSecond);

    // at 32 times wire speed, 2,048 bytes go at once and the rest follow 80 microseconds apart
    auto const elapsed = SendBurst(pacer, 375, 8) - Start;

    CHECK_EQ(elapsed, 119ull * 800ull);

    for (uint32_t multiple : { 2u, 4u, 8u, 16u })
    {
        MidiSendPacer scaled;
        scaled.Configure(multiple, TicksPerSecond);

        auto const scaledElapsed = SendBurst(scaled, 375, 8) - Start;

        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"%ux: %llu ticks", multiple, scaledElapsed));

        VERIFY_IS_LESS_THAN(scaledElapsed, 367ull * 25'600ull, L"faster than wire speed");
        VERIFY_IS_GREATER_THAN(scaledElapsed, 119ull * 800ull, L"slower than 32 times wire speed");
    }
}

void MidiSendPacerTests::TestMessageLargerThanTheAllowanceGoesAtOnce()
{
    MidiSendPacer pacer;
    pacer.Configure(1, TicksPerSecond);

    // more than the 64 bytes which may go at once, on a quiet connection
    CHECK_EQ(pacer.TicksUntilAllowed(500, Start), 0ull);
    pacer.Charge(500, Start);

    // what follows waits until the cable would have caught up, less the allowance
    CHECK_EQ(pacer.TicksUntilAllowed(3, Start), 500ull * 3'200ull + 3ull * 3'200ull - 64ull * 3'200ull);
}

void MidiSendPacerTests::TestResetForgetsWhatWasSent()
{
    MidiSendPacer pacer;
    pacer.Configure(1, TicksPerSecond);

    pacer.Charge(3'000, Start);
    VERIFY_IS_GREATER_THAN(pacer.TicksUntilAllowed(3, Start), 0ull);

    pacer.Reset();
    CHECK_EQ(pacer.TicksUntilAllowed(3, Start), 0ull);

    // a changed limit keeps what was sent
    pacer.Charge(3'000, Start);
    pacer.Configure(2, TicksPerSecond);
    VERIFY_IS_GREATER_THAN(pacer.TicksUntilAllowed(3, Start), 0ull);

    // and no limit at all ignores it
    pacer.Configure(0, TicksPerSecond);
    CHECK_EQ(pacer.TicksUntilAllowed(3, Start), 0ull);
}

void MidiSendPacerTests::TestAnythingAboveTheFastestLimitIsNoLimit()
{
    CHECK_EQ(ClampMidiSendSpeedMultiple(0), 0u);
    CHECK_EQ(ClampMidiSendSpeedMultiple(1), 1u);
    CHECK_EQ(ClampMidiSendSpeedMultiple(3), 3u);
    CHECK_EQ(ClampMidiSendSpeedMultiple(32), 32u);
    CHECK_EQ(ClampMidiSendSpeedMultiple(33), 0u);
    CHECK_EQ(ClampMidiSendSpeedMultiple(0xFFFFFFFF), 0u);

    MidiSendPacer pacer;
    pacer.Configure(64, TicksPerSecond);
    VERIFY_IS_FALSE(pacer.IsLimited());

    MidiAutomaticSendSpeed speed;
    speed.Configure(1000, true);
    CHECK_EQ(speed.CurrentMultiple(), 0u);
}

void MidiSendPacerTests::TestMidi1SizeOfEachMessageType()
{
    // utility: NOOP and JR timestamps never reach a cable
    CHECK_EQ(EstimateMidi1WireByteCount(0x00000000, 1), 0u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x00201234, 1), 0u);

    // system common and real time
    CHECK_EQ(EstimateMidi1WireByteCount(0x10F80000, 1), 1u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x10F61234, 1), 1u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x10F11000, 1), 2u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x10F37F00, 1), 2u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x10F21234, 1), 3u);

    // MIDI 1.0 channel voice
    CHECK_EQ(EstimateMidi1WireByteCount(0x20904540, 1), 3u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x20B30755, 1), 3u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x20C01000, 1), 2u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x20D04000, 1), 2u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x20E00040, 1), 3u);

    // SysEx7: complete, start, continue and end, and a byte count past 6
    CHECK_EQ(EstimateMidi1WireByteCount(0x30037D01, 2), 5u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x30167D01, 2), 7u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x30260102, 2), 6u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x30320102, 2), 3u);
    CHECK_EQ(EstimateMidi1WireByteCount(0x300F0102, 2), 8u);

    // MIDI 2.0 channel voice becomes one MIDI 1.0 message
    CHECK_EQ(EstimateMidi1WireByteCount(0x40904800, 2), 3u);

    // everything else, by size
    CHECK_EQ(EstimateMidi1WireByteCount(0x50000000, 4), 12u);
    CHECK_EQ(EstimateMidi1WireByteCount(0xD0000000, 4), 12u);
    CHECK_EQ(EstimateMidi1WireByteCount(0xF0000000, 4), 12u);
}

void MidiSendPacerTests::TestAutomaticSlowDownStepsDownToWireSpeed()
{
    MidiAutomaticSendSpeed speed;
    speed.Configure(8, true);

    VERIFY_IS_TRUE(speed.IsEnabled());
    CHECK_EQ(speed.CurrentMultiple(), 8u);

    VERIFY_IS_TRUE(speed.OnLoss(Start, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);

    // the requests for one burst of loss count once
    VERIFY_IS_FALSE(speed.OnLoss(Start + TicksPerSecond / 10, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);

    VERIFY_IS_TRUE(speed.OnLoss(Start + TicksPerSecond * 6 / 10, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 2u);

    VERIFY_IS_TRUE(speed.OnLoss(Start + TicksPerSecond * 12 / 10, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 1u);

    // never below wire speed
    VERIFY_IS_FALSE(speed.OnLoss(Start + TicksPerSecond * 2, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 1u);
}

void MidiSendPacerTests::TestAutomaticSlowDownSpeedsBackUp()
{
    MidiAutomaticSendSpeed speed;
    speed.Configure(8, true);

    VERIFY_IS_TRUE(speed.OnLoss(Start, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);

    // ten seconds without loss before each step back up
    VERIFY_IS_FALSE(speed.OnTick(Start + TicksPerSecond * 9, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);

    VERIFY_IS_TRUE(speed.OnTick(Start + TicksPerSecond * 10, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 8u);

    // the chosen limit is as fast as it goes
    VERIFY_IS_FALSE(speed.OnTick(Start + TicksPerSecond * 100, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 8u);

    // a limit which is not a power of two still comes back to itself
    MidiAutomaticSendSpeed odd;
    odd.Configure(6, true);

    VERIFY_IS_TRUE(odd.OnLoss(Start, TicksPerSecond));
    CHECK_EQ(odd.CurrentMultiple(), 3u);

    VERIFY_IS_TRUE(odd.OnTick(Start + TicksPerSecond * 10, TicksPerSecond));
    CHECK_EQ(odd.CurrentMultiple(), 6u);
}

void MidiSendPacerTests::TestAutomaticSlowDownWaitsLongerAfterARaiseFails()
{
    MidiAutomaticSendSpeed speed;
    speed.Configure(4, true);

    VERIFY_IS_TRUE(speed.OnLoss(Start, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 2u);

    VERIFY_IS_TRUE(speed.OnTick(Start + TicksPerSecond * 10, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);

    // the faster speed lost data again straight away
    VERIFY_IS_TRUE(speed.OnLoss(Start + TicksPerSecond * 11, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 2u);

    // so the next try waits twice as long
    VERIFY_IS_FALSE(speed.OnTick(Start + TicksPerSecond * 21, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 2u);

    VERIFY_IS_TRUE(speed.OnTick(Start + TicksPerSecond * 31, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 4u);
}

void MidiSendPacerTests::TestAutomaticSlowDownFromNoLimit()
{
    MidiAutomaticSendSpeed speed;
    speed.Configure(0, true);

    CHECK_EQ(speed.CurrentMultiple(), 0u);

    // the first step down from no limit is the fastest limit
    VERIFY_IS_TRUE(speed.OnLoss(Start, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 32u);

    VERIFY_IS_TRUE(speed.OnLoss(Start + TicksPerSecond, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 16u);

    VERIFY_IS_TRUE(speed.OnTick(Start + TicksPerSecond * 11, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 32u);

    // and past the fastest limit is no limit again
    VERIFY_IS_TRUE(speed.OnTick(Start + TicksPerSecond * 21, TicksPerSecond));
    CHECK_EQ(speed.CurrentMultiple(), 0u);
}

void MidiSendPacerTests::TestAutomaticSlowDownOffChangesNothing()
{
    MidiAutomaticSendSpeed speed;
    speed.Configure(8, false);

    VERIFY_IS_FALSE(speed.IsEnabled());

    VERIFY_IS_FALSE(speed.OnLoss(Start, TicksPerSecond));
    VERIFY_IS_FALSE(speed.OnLoss(Start + TicksPerSecond, TicksPerSecond));
    VERIFY_IS_FALSE(speed.OnTick(Start + TicksPerSecond * 100, TicksPerSecond));

    CHECK_EQ(speed.CurrentMultiple(), 8u);

    // configuring again starts from the chosen speed
    speed.Configure(4, true);
    VERIFY_IS_TRUE(speed.OnLoss(Start, TicksPerSecond));
    speed.Configure(4, true);
    CHECK_EQ(speed.CurrentMultiple(), 4u);
}
