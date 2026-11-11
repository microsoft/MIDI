// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BleIncomingSmoothingTests.h"

#include <cmath>
#include <random>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // 10 MHz, the usual QPC rate
    constexpr uint64_t TicksPerMillisecond = 10'000;
    constexpr uint64_t BaseArrival = 10'000'000'000;

    int64_t Ms(_In_ double const milliseconds)
    {
        return static_cast<int64_t>(std::llround(milliseconds * TicksPerMillisecond));
    }

    double ToMs(_In_ int64_t const ticks)
    {
        return static_cast<double>(ticks) / TicksPerMillisecond;
    }

    // A BLE link as the receiver sees it. A message is sent at the first connection event at or
    // after the moment it was played, and the Windows stack then adds a random delay of its own.
    struct LinkModel
    {
        double IntervalMilliseconds{ 7.5 };
        double StackJitterMilliseconds{ 0.3 };

        // Sender milliseconds per local millisecond. Above 1 is a sender clock running fast.
        double SenderRate{ 1.0 };

        double PhaseMilliseconds{ 3.1 };

        // When set, the interval changes to SwitchIntervalMilliseconds from this time on
        double SwitchAtMilliseconds{ -1.0 };
        double SwitchIntervalMilliseconds{ 0.0 };
    };

    struct SimulatedMessage
    {
        double PlayedMilliseconds{ 0.0 };
        uint64_t Arrival{ 0 };
        uint64_t Release{ 0 };
    };

    std::vector<SimulatedMessage> RunLink(
        _In_ MidiBleSmoothing::IncomingSmoother& smoother,
        _In_ std::vector<double> const& playedMilliseconds,
        _In_ LinkModel const& link,
        _In_ uint32_t const seed = 1)
    {
        std::mt19937 random{ seed };
        std::uniform_real_distribution<double> stack{ 0.0, link.StackJitterMilliseconds };

        std::vector<SimulatedMessage> messages;
        messages.reserve(playedMilliseconds.size());

        size_t index{ 0 };

        while (index < playedMilliseconds.size())
        {
            auto const played = playedMilliseconds[index];

            auto const interval = (link.SwitchAtMilliseconds >= 0.0 && played >= link.SwitchAtMilliseconds) ?
                link.SwitchIntervalMilliseconds : link.IntervalMilliseconds;

            auto const connectionEvent =
                link.PhaseMilliseconds + std::ceil((played - link.PhaseMilliseconds) / interval) * interval;

            std::vector<int64_t> senderTicks;
            std::vector<double> members;

            while (index < playedMilliseconds.size() && playedMilliseconds[index] <= connectionEvent)
            {
                // BLE MIDI 1.0 timestamps are whole milliseconds
                auto const senderMilliseconds = std::floor(100.0 + playedMilliseconds[index] * link.SenderRate);

                senderTicks.push_back(static_cast<int64_t>(senderMilliseconds) * static_cast<int64_t>(TicksPerMillisecond));
                members.push_back(playedMilliseconds[index]);
                index++;
            }

            auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(connectionEvent + stack(random)));

            std::vector<uint64_t> releases;
            smoother.ProcessPacket(senderTicks, arrival, true, TicksPerMillisecond, releases);

            for (size_t i = 0; i < members.size(); i++)
            {
                messages.push_back({ members[i], arrival, releases[i] });
            }
        }

        return messages;
    }

    std::vector<double> SteadyStream(_In_ double const spacing, _In_ double const durationMilliseconds, _In_ double const start = 0.37)
    {
        std::vector<double> played;

        for (double t = start; t < durationMilliseconds; t += spacing)
        {
            played.push_back(t);
        }

        return played;
    }

    // The largest difference between how far apart two messages were played and how far apart
    // they reached the client.
    double MaxSpacingErrorMilliseconds(
        _In_ std::vector<SimulatedMessage> const& messages,
        _In_ double const fromMilliseconds,
        _In_ bool const useRelease)
    {
        double worst{ 0.0 };

        for (size_t i = 1; i < messages.size(); i++)
        {
            if (messages[i - 1].PlayedMilliseconds < fromMilliseconds)
            {
                continue;
            }

            auto const delivered = useRelease ?
                ToMs(static_cast<int64_t>(messages[i].Release - messages[i - 1].Release)) :
                ToMs(static_cast<int64_t>(messages[i].Arrival - messages[i - 1].Arrival));

            auto const played = messages[i].PlayedMilliseconds - messages[i - 1].PlayedMilliseconds;

            worst = (std::max)(worst, std::fabs(delivered - played));
        }

        return worst;
    }

    // How much the time from playing a message to releasing it varies over a stretch
    double LatencySpreadMilliseconds(
        _In_ std::vector<SimulatedMessage> const& messages,
        _In_ double const fromMilliseconds)
    {
        double lowest{ 1e9 };
        double highest{ -1e9 };

        for (auto const& message : messages)
        {
            if (message.PlayedMilliseconds < fromMilliseconds)
            {
                continue;
            }

            auto const latency = ToMs(static_cast<int64_t>(message.Release - BaseArrival)) - message.PlayedMilliseconds;

            lowest = (std::min)(lowest, latency);
            highest = (std::max)(highest, latency);
        }

        return highest - lowest;
    }

    double LateShare(
        _In_ std::vector<SimulatedMessage> const& messages,
        _In_ double const fromMilliseconds)
    {
        size_t total{ 0 };
        size_t late{ 0 };

        for (auto const& message : messages)
        {
            if (message.PlayedMilliseconds < fromMilliseconds)
            {
                continue;
            }

            total++;

            // released the moment it arrived, so it missed its planned release time
            if (message.Release == message.Arrival)
            {
                late++;
            }
        }

        return total == 0 ? 0.0 : static_cast<double>(late) / static_cast<double>(total);
    }

    void VerifyReleasesAreSane(_In_ std::vector<SimulatedMessage> const& messages)
    {
        uint64_t previous{ 0 };

        for (auto const& message : messages)
        {
            VERIFY_IS_TRUE(message.Release >= message.Arrival);
            VERIFY_IS_TRUE(message.Release <= message.Arrival + static_cast<uint64_t>(Ms(MidiBleSmoothing::MaximumDelayMilliseconds)));
            VERIFY_IS_TRUE(message.Release >= previous);

            previous = message.Release;
        }
    }

    // A packet which waited delayMs, with sender time on the local clock so the identity map applies
    void AddDelay(_In_ MidiBleSmoothing::DelayTracker& tracker, _In_ double const delayMs, _In_ double const arrivalMs)
    {
        auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(arrivalMs));

        tracker.Add(static_cast<int64_t>(arrival) - Ms(delayMs), arrival, TicksPerMillisecond);
    }

    double IdentityMap(_In_ int64_t const sender)
    {
        return static_cast<double>(sender);
    }

    // Lower edge samples: delay above the true offset is uniform over one interval
    void FeedEstimator(
        _In_ MidiBleSmoothing::ClockEstimator& estimator,
        _In_ double const durationMilliseconds,
        _In_ double const skew,
        _In_ double const offsetMilliseconds,
        _In_ uint32_t const seed = 7)
    {
        std::mt19937 random{ seed };
        std::uniform_real_distribution<double> delay{ 0.0, 7.5 };

        for (double sender = 0.0; sender < durationMilliseconds; sender += 10.0)
        {
            auto const arrival = sender * (1.0 + skew) + offsetMilliseconds + delay(random);

            estimator.AddSample(Ms(sender), BaseArrival + static_cast<uint64_t>(Ms(arrival)), TicksPerMillisecond);
        }
    }
}


void BleIncomingSmoothingTests::TestClockEstimatorFindsTheLowerEdge()
{
    MidiBleSmoothing::ClockEstimator estimator;

    FeedEstimator(estimator, 5000.0, 0.0, 5.0);

    // The lower edge is the offset plus the smallest delay seen, which over 500 samples spread
    // across 7.5 ms is within a fraction of a millisecond of zero.
    auto const mapped = ToMs(static_cast<int64_t>(std::llround(estimator.Map(Ms(1000.0)))) - static_cast<int64_t>(BaseArrival));

    Log::Comment(String().Format(L"Mapped 1000 ms to %.3f ms", mapped));

    VERIFY_IS_TRUE(mapped >= 1005.0);
    VERIFY_IS_TRUE(mapped < 1005.3);
}

void BleIncomingSmoothingTests::TestClockEstimatorMeasuresSkew()
{
    MidiBleSmoothing::ClockEstimator estimator;

    FeedEstimator(estimator, 30000.0, 0.005, 3.0);

    Log::Comment(String().Format(L"Measured skew %.6f, actual 0.005", estimator.Skew()));

    VERIFY_IS_TRUE(std::fabs(estimator.Skew() - 0.005) < 0.0002);
}

void BleIncomingSmoothingTests::TestClockEstimatorIgnoresSkewOverAShortSpan()
{
    MidiBleSmoothing::ClockEstimator estimator;

    FeedEstimator(estimator, 4000.0, 0.005, 3.0);

    VERIFY_ARE_EQUAL(estimator.Skew(), 0.0);
}

void BleIncomingSmoothingTests::TestClockEstimatorLimitsSkew()
{
    MidiBleSmoothing::ClockEstimator estimator;

    FeedEstimator(estimator, 30000.0, 0.05, 3.0);

    VERIFY_ARE_EQUAL(estimator.Skew(), MidiBleSmoothing::MaximumSkew);
}

void BleIncomingSmoothingTests::TestDelayTrackerIgnoresARareOutlier()
{
    MidiBleSmoothing::DelayTracker tracker;

    for (uint32_t i = 0; i < 1000; i++)
    {
        AddDelay(tracker, 2.0, i * 5.0);
    }

    AddDelay(tracker, 40.0, 5000.0);
    AddDelay(tracker, 40.0, 5005.0);

    VERIFY_ARE_EQUAL(tracker.CoveringDelayTicks(IdentityMap), Ms(2.0));
}

void BleIncomingSmoothingTests::TestDelayTrackerForgetsOldDelays()
{
    MidiBleSmoothing::DelayTracker tracker;

    for (uint32_t i = 0; i < 100; i++)
    {
        AddDelay(tracker, 30.0, i * 10.0);
    }

    VERIFY_ARE_EQUAL(tracker.CoveringDelayTicks(IdentityMap), Ms(30.0));

    for (uint32_t i = 0; i < 100; i++)
    {
        AddDelay(tracker, 2.0, 12000.0 + i * 10.0);
    }

    VERIFY_ARE_EQUAL(tracker.Count(), static_cast<size_t>(100));
    VERIFY_ARE_EQUAL(tracker.CoveringDelayTicks(IdentityMap), Ms(2.0));
}

void BleIncomingSmoothingTests::TestDelayTrackerMeasuresAgainstTheCurrentClock()
{
    // Delays are kept as sender time and arrival, so correcting the clock corrects every delay
    MidiBleSmoothing::DelayTracker tracker;

    for (uint32_t i = 0; i < 100; i++)
    {
        AddDelay(tracker, 30.0, i * 10.0);
    }

    auto const corrected = [](int64_t const sender) { return static_cast<double>(sender) + Ms(28.0); };

    VERIFY_ARE_EQUAL(tracker.CoveringDelayTicks(corrected), Ms(2.0));
}

void BleIncomingSmoothingTests::TestSmootherReleasesOnArrivalWhileWarmingUp()
{
    MidiBleSmoothing::IncomingSmoother smoother;

    for (uint32_t packet = 0; packet + 1 < MidiBleSmoothing::WarmUpPacketCount; packet++)
    {
        std::vector<uint64_t> releases;
        auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(packet * 7.5 + 1.0));

        VERIFY_IS_FALSE(smoother.ProcessPacket({ Ms(packet * 7.5) }, arrival, true, TicksPerMillisecond, releases));
        VERIFY_ARE_EQUAL(releases.size(), static_cast<size_t>(1));
        VERIFY_ARE_EQUAL(releases[0], arrival);
        VERIFY_IS_FALSE(smoother.IsActive());
        VERIFY_ARE_EQUAL(smoother.DelayTicks(), 0LL);
    }

    std::vector<uint64_t> releases;
    VERIFY_IS_TRUE(smoother.ProcessPacket({ Ms(100.0) }, BaseArrival + static_cast<uint64_t>(Ms(101.0)), true, TicksPerMillisecond, releases));
    VERIFY_IS_TRUE(smoother.IsActive());
}

void BleIncomingSmoothingTests::TestSmootherReleasesOnArrivalForAnUnusableClock()
{
    MidiBleSmoothing::IncomingSmoother smoother;

    for (uint32_t packet = 0; packet < 50; packet++)
    {
        std::vector<uint64_t> releases;
        auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(packet * 7.5));

        VERIFY_IS_FALSE(smoother.ProcessPacket({ Ms(0.0), Ms(0.0) }, arrival, false, TicksPerMillisecond, releases));
        VERIFY_ARE_EQUAL(releases.size(), static_cast<size_t>(2));
        VERIFY_ARE_EQUAL(releases[0], arrival);
        VERIFY_ARE_EQUAL(releases[1], arrival);
    }

    VERIFY_IS_FALSE(smoother.IsActive());
}

void BleIncomingSmoothingTests::TestSmootherRemovesConnectionIntervalJitter()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    // 10.3 ms apart, so the millisecond timestamps round differently from one message to the next
    auto const messages = RunLink(smoother, SteadyStream(10.3, 30000.0), LinkModel{});

    auto const arrivalError = MaxSpacingErrorMilliseconds(messages, 5000.0, false);
    auto const releaseError = MaxSpacingErrorMilliseconds(messages, 5000.0, true);

    Log::Comment(String().Format(L"Spacing error: %.3f ms on arrival, %.3f ms released", arrivalError, releaseError));

    VerifyReleasesAreSane(messages);

    // the link really is jittery, or this proves nothing
    VERIFY_IS_TRUE(arrivalError > 4.0);

    // what is left is the 1 ms resolution of the BLE MIDI 1.0 timestamp
    VERIFY_IS_TRUE(releaseError < 1.3);
}

void BleIncomingSmoothingTests::TestSmootherKeepsSpacingWithinAPacket()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(15000);

    // 3 ms apart over a 15 ms interval, so most packets carry several messages
    auto const messages = RunLink(smoother, SteadyStream(3.0, 20000.0), LinkModel{ 15.0, 0.3 });

    auto const releaseError = MaxSpacingErrorMilliseconds(messages, 5000.0, true);

    Log::Comment(String().Format(L"Spacing error %.3f ms released", releaseError));

    VerifyReleasesAreSane(messages);
    VERIFY_IS_TRUE(releaseError < 1.3);
}

void BleIncomingSmoothingTests::TestSmootherFollowsASlowSenderClock()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    LinkModel link{};
    link.SenderRate = 0.99;

    auto const messages = RunLink(smoother, SteadyStream(10.3, 60000.0), link);

    auto const spread = LatencySpreadMilliseconds(messages, 20000.0);
    auto const late = LateShare(messages, 20000.0);

    Log::Comment(String().Format(L"Skew %.5f, latency spread %.3f ms, late %.2f%%", smoother.Skew(), spread, late * 100.0));

    VerifyReleasesAreSane(messages);

    // Without a skew term, a 1% slow clock drifts 50 ms over the 5 s window before stepping
    VERIFY_IS_TRUE(spread < 3.0);
    VERIFY_IS_TRUE(late < 0.02);
}

void BleIncomingSmoothingTests::TestSmootherFollowsAFastSenderClock()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    LinkModel link{};
    link.SenderRate = 1.01;

    auto const messages = RunLink(smoother, SteadyStream(10.3, 60000.0), link);

    auto const spread = LatencySpreadMilliseconds(messages, 20000.0);
    auto const late = LateShare(messages, 20000.0);

    Log::Comment(String().Format(L"Skew %.5f, latency spread %.3f ms, late %.2f%%", smoother.Skew(), spread, late * 100.0));

    VerifyReleasesAreSane(messages);
    VERIFY_IS_TRUE(spread < 3.0);
    VERIFY_IS_TRUE(late < 0.02);
}

void BleIncomingSmoothingTests::TestSmootherDelaySettlesNearTheConnectionInterval()
{
    for (auto const interval : { 7.5, 11.25, 15.0 })
    {
        MidiBleSmoothing::IncomingSmoother smoother;
        smoother.SetConnectionIntervalMicroseconds(static_cast<uint32_t>(interval * 1000));

        LinkModel link{};
        link.IntervalMilliseconds = interval;

        auto const messages = RunLink(smoother, SteadyStream(10.3, 30000.0), link);
        auto const delay = ToMs(smoother.DelayTicks());

        Log::Comment(String().Format(L"Interval %.2f ms: delay %.2f ms, late %.2f%%", interval, delay, LateShare(messages, 5000.0) * 100.0));

        VerifyReleasesAreSane(messages);

        // one interval, plus the timestamp's 1 ms resolution, the stack and the timer guard
        VERIFY_IS_TRUE(delay >= interval);
        VERIFY_IS_TRUE(delay < interval + 3.0);
    }
}

void BleIncomingSmoothingTests::TestSmootherDelayShrinksTowardWhatTheLinkNeeds()
{
    // Interval unknown, so it starts from the 15 ms default
    MidiBleSmoothing::IncomingSmoother smoother;

    auto const messages = RunLink(smoother, SteadyStream(10.3, 30000.0), LinkModel{});
    auto const delay = ToMs(smoother.DelayTicks());

    Log::Comment(String().Format(L"Delay %.2f ms on a 7.5 ms link", delay));

    VerifyReleasesAreSane(messages);
    VERIFY_IS_TRUE(delay < 10.5);

    // and it got there without a jump anyone could hear
    VERIFY_IS_TRUE(MaxSpacingErrorMilliseconds(messages, 2000.0, true) < 1.3);
}

void BleIncomingSmoothingTests::TestSmootherGrowsTheDelayWhenMessagesArriveLate()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    // The link renegotiates to a 30 ms interval part way through and nobody updates the smoother
    LinkModel link{};
    link.SwitchAtMilliseconds = 15000.0;
    link.SwitchIntervalMilliseconds = 30.0;

    auto const messages = RunLink(smoother, SteadyStream(10.3, 30000.0), link);
    auto const delay = ToMs(smoother.DelayTicks());
    auto const late = LateShare(messages, 16000.0);

    Log::Comment(String().Format(L"Delay %.2f ms after the switch, late %.2f%%", delay, late * 100.0));

    VerifyReleasesAreSane(messages);
    VERIFY_IS_TRUE(delay >= 30.0);
    VERIFY_IS_TRUE(late < 0.02);
}

void BleIncomingSmoothingTests::TestSmootherNeverReleasesEarlyOrOutOfOrder()
{
    std::mt19937 random{ 42 };
    std::exponential_distribution<double> gap{ 1.0 / 6.0 };

    // Irregular playing with chords, where several messages share a timestamp
    std::vector<double> played;

    for (double t = 0.0; t < 30000.0; t += gap(random))
    {
        played.push_back(t);
        played.push_back(t);
        played.push_back(t);
    }

    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(11250);

    LinkModel link{};
    link.IntervalMilliseconds = 11.25;
    link.StackJitterMilliseconds = 1.0;

    auto const messages = RunLink(smoother, played, link, 3);

    VerifyReleasesAreSane(messages);
}

void BleIncomingSmoothingTests::TestSmootherNeverHoldsLongerThanTheMaximum()
{
    // Timestamps are untrusted. These jump around with no relation to real time.
    std::mt19937 random{ 9 };
    std::uniform_int_distribution<int64_t> jump{ -3000, 3000 };

    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    int64_t sender{ 100000 };
    uint64_t previous{ 0 };

    for (uint32_t packet = 0; packet < 5000; packet++)
    {
        auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(packet * 7.5));

        sender += Ms(static_cast<double>(jump(random)));

        std::vector<uint64_t> releases;
        smoother.ProcessPacket({ sender, sender + Ms(1.0) }, arrival, true, TicksPerMillisecond, releases);

        for (auto const release : releases)
        {
            VERIFY_IS_TRUE(release >= arrival);
            VERIFY_IS_TRUE(release <= arrival + static_cast<uint64_t>(Ms(MidiBleSmoothing::MaximumDelayMilliseconds)));
            VERIFY_IS_TRUE(release >= previous);

            previous = release;
        }
    }
}

void BleIncomingSmoothingTests::TestSmootherResetClockKeepsTheDelay()
{
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    RunLink(smoother, SteadyStream(10.3, 20000.0), LinkModel{});

    auto const before = smoother.DelayTicks();
    VERIFY_IS_TRUE(before > 0);

    smoother.ResetClock();
    VERIFY_IS_FALSE(smoother.IsActive());

    // The sender restarted its clock from a different value. The link is the same.
    uint64_t const restartArrival = BaseArrival + static_cast<uint64_t>(Ms(25000.0));

    for (uint32_t packet = 0; packet < MidiBleSmoothing::WarmUpPacketCount + 4; packet++)
    {
        std::vector<uint64_t> releases;
        smoother.ProcessPacket({ Ms(packet * 7.5) }, restartArrival + static_cast<uint64_t>(Ms(packet * 7.5 + 2.0)), true, TicksPerMillisecond, releases);
    }

    VERIFY_IS_TRUE(smoother.IsActive());

    auto const after = smoother.DelayTicks();

    Log::Comment(String().Format(L"Delay %.2f ms before, %.2f ms after", ToMs(before), ToMs(after)));

    VERIFY_IS_TRUE(std::llabs(after - before) <= Ms(1.0));
}

void BleIncomingSmoothingTests::TestCorrelatorReportsContinuousSenderTime()
{
    MidiBleMidi1::TimestampCorrelator correlator;

    std::vector<uint64_t> mapped;
    std::vector<uint64_t> sender;

    std::vector<MidiBleMidi1::DecodedSegment> first{ { 0, 8190, { 0x90, 0x3C, 0x7F } } };
    correlator.MapPacket(first, BaseArrival, TicksPerMillisecond, mapped, &sender);

    VERIFY_ARE_EQUAL(sender.size(), static_cast<size_t>(1));
    auto const start = sender[0];

    // across the 13-bit wrap: 8190 -> 5 is 7 ms later
    std::vector<MidiBleMidi1::DecodedSegment> second{ { 0, 5, { 0x80, 0x3C, 0x00 } }, { 1, 6, { 0x90, 0x3E, 0x7F } } };
    correlator.MapPacket(second, BaseArrival + static_cast<uint64_t>(Ms(8.0)), TicksPerMillisecond, mapped, &sender);

    VERIFY_ARE_EQUAL(sender.size(), static_cast<size_t>(2));
    VERIFY_ARE_EQUAL(sender[0], start + 7);
    VERIFY_ARE_EQUAL(sender[1], start + 8);
}

void BleIncomingSmoothingTests::TestCorrelatorClockGenerationChangesOnResynchronize()
{
    MidiBleMidi1::TimestampCorrelator correlator;

    std::vector<uint64_t> mapped;
    std::vector<MidiBleMidi1::DecodedSegment> packet{ { 0, 100, { 0x90, 0x3C, 0x7F } } };

    correlator.MapPacket(packet, BaseArrival, TicksPerMillisecond, mapped);
    auto const generation = correlator.ClockGeneration();

    packet[0].SenderTimestamp = 110;
    correlator.MapPacket(packet, BaseArrival + static_cast<uint64_t>(Ms(10.0)), TicksPerMillisecond, mapped);
    VERIFY_ARE_EQUAL(correlator.ClockGeneration(), generation);

    // longer than the 13-bit clock can express, so the mapping has to start over
    packet[0].SenderTimestamp = 200;
    correlator.MapPacket(packet, BaseArrival + static_cast<uint64_t>(Ms(9000.0)), TicksPerMillisecond, mapped);
    VERIFY_ARE_NOT_EQUAL(correlator.ClockGeneration(), generation);
}

void BleIncomingSmoothingTests::TestSmootherWithTheBleMidi1Correlator()
{
    // The same path as the transport: 13-bit wrapping timestamps through the correlator, its
    // continuous sender time into the smoother.
    MidiBleMidi1::TimestampCorrelator correlator;
    MidiBleSmoothing::IncomingSmoother smoother;
    smoother.SetConnectionIntervalMicroseconds(7500);

    std::mt19937 random{ 5 };
    std::uniform_real_distribution<double> stack{ 0.0, 0.3 };

    auto const played = SteadyStream(10.3, 30000.0);
    std::vector<SimulatedMessage> messages;

    size_t index{ 0 };
    uint32_t generation = correlator.ClockGeneration();

    while (index < played.size())
    {
        auto const connectionEvent = 3.1 + std::ceil((played[index] - 3.1) / 7.5) * 7.5;

        std::vector<MidiBleMidi1::DecodedSegment> segments;
        std::vector<double> members;

        while (index < played.size() && played[index] <= connectionEvent)
        {
            auto const senderMilliseconds = static_cast<uint32_t>(std::floor(100.0 + played[index]));

            segments.push_back({ 0, static_cast<uint16_t>(senderMilliseconds & 0x1FFF), { 0x90, 0x3C, 0x7F } });
            members.push_back(played[index]);
            index++;
        }

        auto const arrival = BaseArrival + static_cast<uint64_t>(Ms(connectionEvent + stack(random)));

        std::vector<uint64_t> mapped;
        std::vector<uint64_t> senderMilliseconds;
        correlator.MapPacket(segments, arrival, TicksPerMillisecond, mapped, &senderMilliseconds);

        if (correlator.ClockGeneration() != generation)
        {
            smoother.ResetClock();
            generation = correlator.ClockGeneration();
        }

        std::vector<int64_t> senderTicks;

        for (auto const milliseconds : senderMilliseconds)
        {
            senderTicks.push_back(static_cast<int64_t>(milliseconds * TicksPerMillisecond));
        }

        std::vector<uint64_t> releases;
        smoother.ProcessPacket(
            senderTicks,
            arrival,
            correlator.HaveSeenSenderClockAdvance() && !correlator.SenderClockIsStalled(),
            TicksPerMillisecond,
            releases);

        for (size_t i = 0; i < members.size(); i++)
        {
            messages.push_back({ members[i], arrival, releases[i] });
        }
    }

    auto const releaseError = MaxSpacingErrorMilliseconds(messages, 5000.0, true);

    Log::Comment(String().Format(L"Spacing error %.3f ms released, delay %.2f ms", releaseError, ToMs(smoother.DelayTicks())));

    VerifyReleasesAreSane(messages);

    // 30 s crosses the 8.192 s wrap three times without losing the mapping
    VERIFY_IS_TRUE(smoother.IsActive());
    VERIFY_IS_TRUE(releaseError < 1.3);
}
