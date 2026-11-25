// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PlaybackClockTests.h"

#include "midi_sequence_playback_clock.h"

#include <windows.h>

#include <deque>
#include <limits>
#include <random>
#include <utility>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using midiplayer::PlaybackClock;

namespace
{
    // A typical high resolution timer frequency, which is what the MIDI clock counts in.
    constexpr uint64_t Frequency = 10'000'000;

    constexpr uint64_t TicksFromMilliseconds(_In_ uint64_t milliseconds) noexcept
    {
        return milliseconds * Frequency / 1000;
    }

    constexpr uint64_t MicrosecondsFromMilliseconds(_In_ uint64_t milliseconds) noexcept
    {
        return milliseconds * 1000;
    }

    // A day into a running machine's clock, so nothing here depends on the clock starting at zero.
    constexpr uint64_t Boot = TicksFromMilliseconds(24ull * 60 * 60 * 1000);

    // The playback engine's own figures: the lead before a start, how often its worker makes a
    // pass, and how far ahead of the clock each pass hands messages to the service.
    constexpr uint64_t StartLeadTicks = TicksFromMilliseconds(60);
    constexpr uint64_t PassTicks = TicksFromMilliseconds(40);
    constexpr uint64_t LookAheadTicks = TicksFromMilliseconds(250);

    // Each conversion rounds down by less than a microsecond. A design that gets the timing wrong
    // is out by hundreds of milliseconds, so this cannot hide one.
    constexpr uint64_t ToleranceMicroseconds = 20;

    uint64_t Difference(_In_ uint64_t first, _In_ uint64_t second) noexcept
    {
        return first > second ? first - second : second - first;
    }
}

void PlaybackClockTests::MatchesTheOriginalArithmeticAtTheWrittenSpeed()
{
    PlaybackClock clock{};

    auto const origin = Boot + StartLeadTicks;
    auto const originMicroseconds = uint64_t{ 1'234'567 };

    clock.Start(Frequency, origin, originMicroseconds);

    VERIFY_ARE_EQUAL(1.0, clock.Rate());

    // What the engine worked out before it had a rate, written out again here so that a change to
    // the clock cannot quietly change playback at the written speed.
    auto const originalTimestamp = [&](uint64_t microseconds)
        {
            if (microseconds <= originMicroseconds)
            {
                return origin;
            }

            return origin + (microseconds - originMicroseconds) * Frequency / 1000000;
        };

    auto const originalPosition = [&](uint64_t timestamp)
        {
            if (timestamp <= origin)
            {
                return originMicroseconds;
            }

            return originMicroseconds + ((timestamp - origin) * 1000000 / Frequency);
        };

    for (auto const microseconds : {
        uint64_t{ 0 },
        originMicroseconds - 1,
        originMicroseconds,
        originMicroseconds + 1,
        originMicroseconds + 999,
        originMicroseconds + 1'000'000,
        originMicroseconds + 3'600'000'000 })
    {
        VERIFY_ARE_EQUAL(originalTimestamp(microseconds), clock.TimestampForMicroseconds(microseconds));
    }

    for (auto const timestamp : {
        Boot,
        origin - 1,
        origin,
        origin + 1,
        origin + 7,
        origin + TicksFromMilliseconds(1000),
        origin + TicksFromMilliseconds(3'600'000) })
    {
        VERIFY_ARE_EQUAL(originalPosition(timestamp), clock.MicrosecondsAtTimestamp(timestamp));
    }
}

void PlaybackClockTests::HoldsThePositionDuringTheStartLead()
{
    PlaybackClock clock{};

    clock.Start(Frequency, Boot + StartLeadTicks, MicrosecondsFromMilliseconds(5000));

    // Nothing has started yet, so the position is where playback starts from, not some way before.
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(5000), clock.MicrosecondsAtTimestamp(Boot));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(5000), clock.MicrosecondsAtTimestamp(Boot + StartLeadTicks));

    VERIFY_IS_TRUE(clock.RequestRate(0.5));

    clock.Start(Frequency, Boot + StartLeadTicks, MicrosecondsFromMilliseconds(5000));

    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(5000), clock.MicrosecondsAtTimestamp(Boot));
}

void PlaybackClockTests::ScalesTimeByTheRate()
{
    PlaybackClock clock{};

    VERIFY_IS_TRUE(clock.RequestRate(0.5));

    clock.Start(Frequency, Boot, 0);

    VERIFY_ARE_EQUAL(0.5, clock.Rate());
    VERIFY_IS_FALSE(clock.IsRateChangePending());

    // A second of music takes two at half speed...
    VERIFY_ARE_EQUAL(Boot + TicksFromMilliseconds(2000), clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(1000)));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(1000), clock.MicrosecondsAtTimestamp(Boot + TicksFromMilliseconds(2000)));

    VERIFY_IS_TRUE(clock.RequestRate(2.0));

    clock.Start(Frequency, Boot, 0);

    // ...and half of one at double speed.
    VERIFY_ARE_EQUAL(Boot + TicksFromMilliseconds(500), clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(1000)));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(1000), clock.MicrosecondsAtTimestamp(Boot + TicksFromMilliseconds(500)));

    // A rate with no exact binary form still lands within a tick and a microsecond.
    VERIFY_IS_TRUE(clock.RequestRate(0.3));

    clock.Start(Frequency, Boot, 0);

    auto const timestamp = clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(3000));

    VERIFY_IS_TRUE(Difference(Boot + TicksFromMilliseconds(10000), timestamp) <= 1);
    VERIFY_IS_TRUE(Difference(MicrosecondsFromMilliseconds(3000), clock.MicrosecondsAtTimestamp(timestamp)) <= 1);
}

void PlaybackClockTests::ClampsTheRateAndIgnoresValuesThatAreNotNumbers()
{
    PlaybackClock clock{};

    VERIFY_ARE_EQUAL(1.0, clock.RequestedRate());

    VERIFY_IS_TRUE(clock.RequestRate(10.0));
    VERIFY_ARE_EQUAL(PlaybackClock::MaximumRate, clock.RequestedRate());

    VERIFY_IS_TRUE(clock.RequestRate(0.01));
    VERIFY_ARE_EQUAL(PlaybackClock::MinimumRate, clock.RequestedRate());

    VERIFY_IS_TRUE(clock.RequestRate(0.0));
    VERIFY_ARE_EQUAL(PlaybackClock::MinimumRate, clock.RequestedRate());

    VERIFY_IS_TRUE(clock.RequestRate(-1.0));
    VERIFY_ARE_EQUAL(PlaybackClock::MinimumRate, clock.RequestedRate());

    VERIFY_IS_TRUE(clock.RequestRate(0.75));

    VERIFY_IS_FALSE(clock.RequestRate(std::numeric_limits<double>::quiet_NaN()));
    VERIFY_IS_FALSE(clock.RequestRate(std::numeric_limits<double>::infinity()));
    VERIFY_IS_FALSE(clock.RequestRate(-std::numeric_limits<double>::infinity()));

    VERIFY_ARE_EQUAL(0.75, clock.RequestedRate());
}

void PlaybackClockTests::StartsANewRateAtTheEndOfWhatWasHandedOver()
{
    PlaybackClock clock{};

    clock.Start(Frequency, Boot, 0);

    // A pass 100 ms in hands over everything due in the next 250 ms.
    auto const now = Boot + TicksFromMilliseconds(100);
    auto const horizon = now + LookAheadTicks;

    clock.MarkScheduledThrough(horizon);

    VERIFY_IS_TRUE(clock.RequestRate(0.5));
    VERIFY_IS_TRUE(clock.IsRateChangePending());

    VERIFY_IS_TRUE(clock.ApplyRequestedRate(now));
    VERIFY_IS_FALSE(clock.IsRateChangePending());
    VERIFY_ARE_EQUAL(0.5, clock.Rate());

    // What is already with the service still plays at full speed, so the position does too.
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(100), clock.MicrosecondsAtTimestamp(now));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(200), clock.MicrosecondsAtTimestamp(now + TicksFromMilliseconds(100)));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(350), clock.MicrosecondsAtTimestamp(horizon));

    // Half speed starts exactly where that ends.
    VERIFY_ARE_EQUAL(horizon, clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(350)));
    VERIFY_ARE_EQUAL(horizon + TicksFromMilliseconds(200), clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(450)));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(450), clock.MicrosecondsAtTimestamp(horizon + TicksFromMilliseconds(200)));

    // Nothing can be scheduled ahead of what was already handed over, even if it were asked for.
    VERIFY_ARE_EQUAL(horizon, clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(300)));
}

void PlaybackClockTests::HoldsASecondChangeUntilTheFirstHasStarted()
{
    PlaybackClock clock{};

    clock.Start(Frequency, Boot, 0);

    // One pass of the playback worker, in the order the engine makes it.
    auto const pass = [&clock](uint64_t milliseconds)
        {
            auto const now = Boot + TicksFromMilliseconds(milliseconds);
            auto const changed = clock.ApplyRequestedRate(now);

            clock.MarkScheduledThrough(now + LookAheadTicks);

            return changed;
        };

    VERIFY_IS_FALSE(pass(100));

    VERIFY_IS_TRUE(clock.RequestRate(0.5));

    // Half speed, from 350 ms, where the pass at 100 ms handed over to.
    VERIFY_IS_TRUE(pass(140));

    VERIFY_IS_TRUE(clock.RequestRate(2.0));

    // Half speed has not started yet, and the stretch before it has to stay at one rate, so double
    // speed waits for it.
    for (uint64_t milliseconds = 180; milliseconds < 350; milliseconds += 40)
    {
        VERIFY_IS_FALSE(pass(milliseconds));
        VERIFY_IS_TRUE(clock.IsRateChangePending());
        VERIFY_ARE_EQUAL(0.5, clock.Rate());
    }

    // The pass at 340 ms handed over to 590 ms, so that is where double speed starts.
    VERIFY_IS_TRUE(pass(380));
    VERIFY_ARE_EQUAL(2.0, clock.Rate());

    // Full speed to 350 ms, then half speed for the 240 ms to 590 ms: 470 ms into the music.
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(470), clock.MicrosecondsAtTimestamp(Boot + TicksFromMilliseconds(590)));
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(365), clock.MicrosecondsAtTimestamp(Boot + TicksFromMilliseconds(380)));
    VERIFY_ARE_EQUAL(Boot + TicksFromMilliseconds(640), clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(570)));
}

void PlaybackClockTests::AppliesAWaitingChangeAtTheNextStart()
{
    PlaybackClock clock{};

    clock.Start(Frequency, Boot, 0);
    clock.MarkScheduledThrough(Boot + LookAheadTicks);

    VERIFY_IS_TRUE(clock.RequestRate(0.5));
    VERIFY_IS_TRUE(clock.ApplyRequestedRate(Boot + TicksFromMilliseconds(10)));

    // Asked for while playing, then stopped or moved before the worker got to it.
    VERIFY_IS_TRUE(clock.RequestRate(2.0));
    VERIFY_IS_TRUE(clock.IsRateChangePending());

    clock.Start(Frequency, Boot + TicksFromMilliseconds(1000), MicrosecondsFromMilliseconds(4000));

    VERIFY_IS_FALSE(clock.IsRateChangePending());
    VERIFY_ARE_EQUAL(2.0, clock.Rate());

    // The lead before a new start holds still, whatever rate came before it.
    VERIFY_ARE_EQUAL(MicrosecondsFromMilliseconds(4000), clock.MicrosecondsAtTimestamp(Boot + TicksFromMilliseconds(950)));
    VERIFY_ARE_EQUAL(Boot + TicksFromMilliseconds(1500), clock.TimestampForMicroseconds(MicrosecondsFromMilliseconds(5000)));
}

void PlaybackClockTests::KeepsEveryMessageInOrderWhileTheRateMoves()
{
    // Two minutes of busy music, about one message every 12 ms, played by the same passes the
    // engine makes, while the speed slider never stops moving: a step every 5 to 90 ms, and now
    // and then a jump from one end to the other. Three different runs.
    for (uint32_t seed = 1; seed <= 3; ++seed)
    {
        std::mt19937 random{ seed };
        std::uniform_real_distribution<double> unit{ 0.0, 1.0 };
        std::exponential_distribution<double> gap{ 1.0 / 12000.0 };

        std::vector<uint64_t> messages{};

        for (double at = 0.0; at < 120'000'000.0; at += gap(random))
        {
            messages.push_back(static_cast<uint64_t>(at));
        }

        PlaybackClock clock{};

        auto now = Boot;

        clock.Start(Frequency, now + StartLeadTicks, 0);

        auto requested = 1.0;
        auto nextRequest = now + TicksFromMilliseconds(500);

        // Handed over and not due yet: when it is due, and where it is in the music.
        std::deque<std::pair<uint64_t, uint64_t>> scheduled{};

        size_t next{ 0 };
        size_t changes{ 0 };
        size_t outOfOrder{ 0 };
        size_t late{ 0 };
        size_t misplaced{ 0 };
        size_t jumps{ 0 };
        size_t backwards{ 0 };

        uint64_t lastHandedOver{ 0 };
        uint64_t furthestPosition{ 0 };
        auto previousPass = now;

        // At a quarter of the speed, two minutes of music takes eight.
        auto const deadline = now + TicksFromMilliseconds(30 * 60 * 1000);

        while ((next < messages.size() || !scheduled.empty()) && now < deadline)
        {
            // Between passes the position only ever moves forward.
            for (auto at = previousPass; at < now; at += TicksFromMilliseconds(1))
            {
                auto const position = clock.MicrosecondsAtTimestamp(at);

                if (position + ToleranceMicroseconds < furthestPosition)
                {
                    ++backwards;
                }

                furthestPosition = (std::max)(furthestPosition, position);
            }

            // Everything that came due since the last pass sounded where it sits in the music.
            while (!scheduled.empty() && scheduled.front().first <= now)
            {
                if (Difference(clock.MicrosecondsAtTimestamp(scheduled.front().first), scheduled.front().second) > ToleranceMicroseconds)
                {
                    ++misplaced;
                }

                scheduled.pop_front();
            }

            while (nextRequest <= now)
            {
                if (unit(random) < 0.05)
                {
                    requested = unit(random) < 0.5 ? PlaybackClock::MinimumRate : PlaybackClock::MaximumRate;
                }
                else
                {
                    requested = std::clamp(
                        requested + (unit(random) < 0.5 ? -0.05 : 0.05),
                        PlaybackClock::MinimumRate,
                        PlaybackClock::MaximumRate);
                }

                clock.RequestRate(requested);

                nextRequest += TicksFromMilliseconds(5) + static_cast<uint64_t>(unit(random) * static_cast<double>(TicksFromMilliseconds(85)));
            }

            // One pass of the playback worker.
            auto const before = clock.MicrosecondsAtTimestamp(now);

            if (clock.ApplyRequestedRate(now))
            {
                ++changes;
            }

            if (Difference(before, clock.MicrosecondsAtTimestamp(now)) > ToleranceMicroseconds)
            {
                ++jumps;
            }

            auto const horizon = now + LookAheadTicks;

            while (next < messages.size())
            {
                auto const timestamp = clock.TimestampForMicroseconds(messages[next]);

                if (timestamp > horizon)
                {
                    break;
                }

                if (timestamp < now)
                {
                    ++late;
                }

                auto const handedOver = (std::max)(timestamp, now);

                if (handedOver < lastHandedOver)
                {
                    ++outOfOrder;
                }

                lastHandedOver = handedOver;
                scheduled.emplace_back(handedOver, messages[next]);

                ++next;
            }

            clock.MarkScheduledThrough(horizon);

            previousPass = now;
            now += PassTicks;
        }

        Log::Comment(String().Format(
            L"Run %u: %d messages, %d speed changes. Out of order %d, late %d, misplaced %d, position jumps %d, position went backward %d.",
            seed,
            static_cast<int>(messages.size()),
            static_cast<int>(changes),
            static_cast<int>(outOfOrder),
            static_cast<int>(late),
            static_cast<int>(misplaced),
            static_cast<int>(jumps),
            static_cast<int>(backwards)));

        VERIFY_ARE_EQUAL(messages.size(), next);
        VERIFY_IS_TRUE(scheduled.empty());

        // Otherwise the run proved nothing about changing speed.
        VERIFY_IS_TRUE(changes > 100);

        VERIFY_ARE_EQUAL(size_t{ 0 }, outOfOrder);
        VERIFY_ARE_EQUAL(size_t{ 0 }, late);
        VERIFY_ARE_EQUAL(size_t{ 0 }, misplaced);
        VERIFY_ARE_EQUAL(size_t{ 0 }, jumps);
        VERIFY_ARE_EQUAL(size_t{ 0 }, backwards);
    }
}
