// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_INCOMING_SMOOTHING_H
#define MIDI_BLE_INCOMING_SMOOTHING_H

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

// Works out when each incoming message should be handed to clients so that they get the spacing
// the sender played, not the spacing the radio delivered.
//
// A BLE link only carries data at connection events, one every connection interval. A message
// waits anywhere from no time to a whole interval for the next event, longer when the link skips
// events or retransmits, and then the Windows Bluetooth stack adds its own variable delay. Handed
// on as each packet arrives, messages pick up all of that as jitter. The sender stamps every
// message with its own clock, and that is enough to remove the jitter once its clock has been
// lined up with ours.
//
// The model is arrival = offset + (1 + skew) * senderTime + delay, where delay is never negative.
// Plotting (senderTime, arrival - senderTime) for every packet, the samples with the least delay
// form a straight lower edge, and that edge is the sender's clock mapped onto ours. It is found as
// the lower convex hull of the smallest sample in each one second bucket, using the hull edge
// above the middle of the window, which is the line under every sample with the smallest average
// gap above it. A plain sliding minimum has no skew term, so a sender clock running slow by 1%
// would leave it up to 50 ms behind over a 5 second window before stepping. That is harmless
// when timestamps are only back-dated, but not once they decide when a message is delivered.
//
// Each message is then released at its mapped time plus a delay just long enough that almost no
// message arrives after its own release time. That delay is measured, not assumed: it follows the
// spread of delays the link is actually producing, measured against the clock as it is understood
// now. It rises at once when messages start arriving late and falls back in small steps, so it
// settles at about one connection interval on a healthy link with a good sender clock.
//
// No Bluetooth, service or COM dependency, so it is unit tested on its own. Everything is in local
// timestamp ticks, and the protocol converts sender times to ticks first, so the same code can
// serve the millisecond clock of BLE MIDI 1.0 and the JR Clock of BLE MIDI 2.0.
namespace MidiBleSmoothing
{
    // The lower edge is built from the smallest delay seen in each bucket of this much local time.
    inline constexpr uint32_t ClockBucketMilliseconds = 1000;

    // How far back the lower edge looks. Long enough to measure skew well, short enough to follow
    // a sender clock whose rate changes as it warms up.
    inline constexpr uint32_t ClockWindowMilliseconds = 30000;

    // Skew is left at zero until the samples span this much sender time, because a slope measured
    // over a second or two is mostly noise from where in the connection interval each sample
    // landed. Before then, an ordinary crystal (a few tens of parts per million) drifts well
    // under a millisecond.
    inline constexpr uint32_t SkewMinimumSpanMilliseconds = 5000;

    // A crystal is within a few hundred parts per million. Anything past 2% is not a clock running
    // at a different rate, it's a broken one, and following it would wreck the timing.
    inline constexpr double MaximumSkew = 0.02;

    // Messages are delivered as they arrive until the sender's clock has been sampled this many
    // times, because a lower edge built from a handful of packets can be well above the real one.
    inline constexpr uint32_t WarmUpPacketCount = 8;

    // The delay is chosen from the packets seen over this much local time.
    inline constexpr uint32_t DelayWindowMilliseconds = 10000;

    // The delay covers all but this share of packets, in parts per ten thousand (0.5%). Covering
    // every last one would let a single retransmitted packet hold the delay up for the whole
    // window, and a message that is a little late is delivered the moment it arrives anyway.
    inline constexpr uint32_t LateAllowancePerTenThousand = 50;

    // Added to the measured delay to cover how late the release timer can wake.
    inline constexpr uint32_t DelayGuardMicroseconds = 500;

    inline constexpr uint32_t MinimumDelayMicroseconds = 1000;

    // Never hold a message longer than this. A sender whose timestamps put it this far from the
    // lower edge is not something smoothing can fix, and timestamps are untrusted input.
    inline constexpr uint32_t MaximumDelayMilliseconds = 50;

    // Before any delays have been measured, start at one connection interval plus this.
    inline constexpr uint32_t InitialDelayMarginMilliseconds = 2;

    // Used when the connection interval is not known. The largest interval both specifications allow.
    inline constexpr uint32_t DefaultConnectionIntervalMicroseconds = 15000;

    // How often the delay is measured again, unless a packet arrives later than the delay allows,
    // which measures it at once.
    inline constexpr uint32_t DelayMeasureIntervalMilliseconds = 50;

    // Shrinking closes an eighth of the gap to what the link needs each time, never less than the
    // smallest step and never more than the largest, so the delay settles without a jump anyone
    // could hear. Each step moves the next message's release earlier by that much.
    inline constexpr uint32_t ShrinkIntervalMilliseconds = 100;
    inline constexpr uint32_t ShrinkDivisor = 8;
    inline constexpr uint32_t SmallestShrinkStepMicroseconds = 100;
    inline constexpr uint32_t LargestShrinkStepMicroseconds = 500;

    // The delay doesn't shrink until it has this many packets to go on.
    inline constexpr uint32_t MinimumDelaySamplesBeforeShrinking = 32;

    // A bound on memory for a device which sends far more packets than any real one could.
    inline constexpr size_t MaximumDelaySamples = 16384;

    inline int64_t MicrosecondsToTicks(_In_ uint64_t const microseconds, _In_ uint64_t const ticksPerMillisecond) noexcept
    {
        return static_cast<int64_t>((microseconds * ticksPerMillisecond) / 1000);
    }

    // Maps sender times onto local time through the lower edge of the delay samples.
    class ClockEstimator
    {
    public:
        void Reset() noexcept
        {
            m_buckets.clear();
            m_haveBase = false;
            m_baseSenderTicks = 0;
            m_baseArrivalTicks = 0;
            m_sampleCount = 0;
            m_slope = 0.0;
            m_intercept = 0.0;
        }

        uint32_t SampleCount() const noexcept { return m_sampleCount; }

        // A sender clock running slow relative to ours is positive, fast is negative.
        double Skew() const noexcept { return m_slope; }

        // One sample per packet: the newest sender time in it, against when it arrived. Sender times
        // are expected not to go backwards; a sender clock which restarts needs a Reset instead.
        void AddSample(
            _In_ int64_t const senderTicks,
            _In_ uint64_t const arrivalTicks,
            _In_ uint64_t const ticksPerMillisecond)
        {
            if (ticksPerMillisecond == 0 || (m_haveBase && arrivalTicks < m_baseArrivalTicks))
            {
                return;
            }

            if (!m_haveBase)
            {
                // Kept relative to the first sample so the arithmetic stays well inside what a
                // double holds exactly, however long the link has been up.
                m_baseSenderTicks = senderTicks;
                m_baseArrivalTicks = arrivalTicks;
                m_haveBase = true;
            }

            double const x = static_cast<double>(senderTicks - m_baseSenderTicks);
            double const y = static_cast<double>(arrivalTicks - m_baseArrivalTicks) - x;

            uint64_t const bucketTicks = static_cast<uint64_t>(ClockBucketMilliseconds) * ticksPerMillisecond;
            uint64_t const bucketIndex = (arrivalTicks - m_baseArrivalTicks) / bucketTicks;

            if (!m_buckets.empty() && m_buckets.back().Index == bucketIndex)
            {
                if (y < m_buckets.back().Y)
                {
                    m_buckets.back().X = x;
                    m_buckets.back().Y = y;
                }
            }
            else
            {
                m_buckets.push_back({ bucketIndex, x, y });
            }

            uint64_t const windowBuckets = ClockWindowMilliseconds / ClockBucketMilliseconds;

            while (m_buckets.size() > 1 && bucketIndex - m_buckets.front().Index >= windowBuckets)
            {
                m_buckets.pop_front();
            }

            if (m_sampleCount < UINT32_MAX)
            {
                m_sampleCount++;
            }

            Fit(ticksPerMillisecond);
        }

        // Where the sender's time falls on the local clock, before any delay is added.
        double Map(_In_ int64_t const senderTicks) const noexcept
        {
            double const x = static_cast<double>(senderTicks - m_baseSenderTicks);

            return static_cast<double>(m_baseArrivalTicks) + x + m_intercept + m_slope * x;
        }

    private:
        struct Point
        {
            uint64_t Index{ 0 };
            double X{ 0.0 };
            double Y{ 0.0 };
        };

        static double Cross(_In_ Point const& o, _In_ Point const& a, _In_ Point const& b) noexcept
        {
            return (a.X - o.X) * (b.Y - o.Y) - (a.Y - o.Y) * (b.X - o.X);
        }

        void Fit(_In_ uint64_t const ticksPerMillisecond)
        {
            m_hull.clear();

            // Sender times don't go backwards, so the buckets are already ordered by X. A sample
            // directly above another at the same X can never be on the lower edge.
            for (auto const& point : m_buckets)
            {
                if (!m_hull.empty() && m_hull.back().X == point.X)
                {
                    if (point.Y >= m_hull.back().Y)
                    {
                        continue;
                    }

                    m_hull.pop_back();
                }

                while (m_hull.size() >= 2 && Cross(m_hull[m_hull.size() - 2], m_hull.back(), point) <= 0.0)
                {
                    m_hull.pop_back();
                }

                m_hull.push_back(point);
            }

            double slope{ 0.0 };

            double const span = m_buckets.back().X - m_buckets.front().X;
            double const minimumSpan = static_cast<double>(SkewMinimumSpanMilliseconds) * static_cast<double>(ticksPerMillisecond);

            if (m_hull.size() >= 2 && span >= minimumSpan)
            {
                double meanX{ 0.0 };

                for (auto const& point : m_buckets)
                {
                    meanX += point.X;
                }

                meanX /= static_cast<double>(m_buckets.size());

                for (size_t i = 0; i + 1 < m_hull.size(); i++)
                {
                    if (m_hull[i + 1].X >= meanX)
                    {
                        slope = (m_hull[i + 1].Y - m_hull[i].Y) / (m_hull[i + 1].X - m_hull[i].X);
                        break;
                    }
                }

                slope = (std::clamp)(slope, -MaximumSkew, MaximumSkew);
            }

            // Whatever the slope, the line is lowered until it touches the lowest sample, so the
            // mapped time of a packet is never after the time it arrived.
            double intercept = m_buckets.front().Y - slope * m_buckets.front().X;

            for (auto const& point : m_buckets)
            {
                intercept = (std::min)(intercept, point.Y - slope * point.X);
            }

            m_slope = slope;
            m_intercept = intercept;
        }

        std::deque<Point> m_buckets{ };
        std::vector<Point> m_hull{ };

        bool m_haveBase{ false };
        int64_t m_baseSenderTicks{ 0 };
        uint64_t m_baseArrivalTicks{ 0 };
        uint32_t m_sampleCount{ 0 };

        double m_slope{ 0.0 };
        double m_intercept{ 0.0 };
    };


    // The packets of the last few seconds, kept as sender time and arrival rather than as delays,
    // so that how late each one was is always measured against the current understanding of the
    // sender's clock. Kept as delays, a correction to the clock (skew found, a lower sample seen)
    // would leave stale numbers holding the delay up until they aged out.
    class DelayTracker
    {
    public:
        void Reset() noexcept
        {
            m_samples.clear();
        }

        size_t Count() const noexcept { return m_samples.size(); }

        // One per packet: the sender time of its oldest message, which waited longest, and when
        // the packet arrived.
        void Add(
            _In_ int64_t const senderTicks,
            _In_ uint64_t const arrivalTicks,
            _In_ uint64_t const ticksPerMillisecond)
        {
            m_samples.push_back({ arrivalTicks, senderTicks });

            uint64_t const windowTicks = static_cast<uint64_t>(DelayWindowMilliseconds) * ticksPerMillisecond;

            while (!m_samples.empty() &&
                (m_samples.size() > MaximumDelaySamples || arrivalTicks - m_samples.front().ArrivalTicks > windowTicks))
            {
                m_samples.pop_front();
            }
        }

        // The smallest delay all but the allowed share of packets fit under. map turns a sender
        // time into local ticks.
        template <typename MapFunction>
        int64_t CoveringDelayTicks(_In_ MapFunction const& map)
        {
            if (m_samples.empty())
            {
                return 0;
            }

            m_scratch.clear();
            m_scratch.reserve(m_samples.size());

            for (auto const& sample : m_samples)
            {
                m_scratch.push_back(static_cast<int64_t>(sample.ArrivalTicks) - static_cast<int64_t>(std::llround(map(sample.SenderTicks))));
            }

            size_t const allowedLate = (m_scratch.size() * LateAllowancePerTenThousand) / 10000;
            size_t const index = m_scratch.size() - 1 - (std::min)(allowedLate, m_scratch.size() - 1);

            std::nth_element(m_scratch.begin(), m_scratch.begin() + index, m_scratch.end());

            return (std::max)(m_scratch[index], static_cast<int64_t>(0));
        }

    private:
        struct Sample
        {
            uint64_t ArrivalTicks{ 0 };
            int64_t SenderTicks{ 0 };
        };

        std::deque<Sample> m_samples{ };
        std::vector<int64_t> m_scratch{ };
    };


    // Turns the sender times in each packet into release times.
    class IncomingSmoother
    {
    public:
        // The link dropped, so nothing learned about it applies any more.
        void Reset() noexcept
        {
            m_clock.Reset();
            m_delays.Reset();
            m_delayTicks = 0;
            m_haveTarget = false;
            m_active = false;
            m_lastReleaseTicks = 0;
            m_lastShrinkArrivalTicks = 0;
        }

        // The sender's clock restarted or jumped. The mapping is rebuilt, but the delay the link
        // needs hasn't changed, so it is kept. Old packets can't be measured against the new
        // clock, so they're dropped.
        void ResetClock() noexcept
        {
            m_clock.Reset();
            m_delays.Reset();
            m_haveTarget = false;
            m_active = false;
        }

        // Zero means unknown.
        void SetConnectionIntervalMicroseconds(_In_ uint32_t const microseconds) noexcept
        {
            m_connectionIntervalMicroseconds = microseconds;
        }

        // False while warming up or when the sender's clock can't be used, which is when messages
        // are released as they arrive.
        bool IsActive() const noexcept { return m_active; }

        // What smoothing adds on top of the mapped time right now. Zero when not active.
        int64_t DelayTicks() const noexcept { return m_active ? m_delayTicks : 0; }

        double Skew() const noexcept { return m_clock.Skew(); }

        uint64_t LateMessageCount() const noexcept { return m_lateMessageCount; }

        // senderTicks holds one sender time per message, oldest first, already converted to local
        // ticks and continuous across packets. releaseTicks gets one release time per message.
        // Release times never go backwards and are never before the packet arrived. Returns false
        // when every release time is simply the arrival time.
        bool ProcessPacket(
            _In_ std::vector<int64_t> const& senderTicks,
            _In_ uint64_t const arrivalTicks,
            _In_ bool const senderClockUsable,
            _In_ uint64_t const ticksPerMillisecond,
            _Inout_ std::vector<uint64_t>& releaseTicks)
        {
            releaseTicks.clear();

            if (senderTicks.empty() || ticksPerMillisecond == 0)
            {
                return false;
            }

            if (!senderClockUsable)
            {
                if (m_clock.SampleCount() != 0)
                {
                    ResetClock();
                }

                m_active = false;

                ReleaseAllAtArrival(senderTicks.size(), arrivalTicks, releaseTicks);

                return false;
            }

            m_clock.AddSample(senderTicks.back(), arrivalTicks, ticksPerMillisecond);
            m_delays.Add(senderTicks.front(), arrivalTicks, ticksPerMillisecond);

            if (m_clock.SampleCount() < WarmUpPacketCount)
            {
                m_active = false;

                ReleaseAllAtArrival(senderTicks.size(), arrivalTicks, releaseTicks);

                return false;
            }

            // The oldest message in the packet waited longest, so it is the one the delay has to
            // cover for nothing in the packet to be late.
            int64_t const oldestDelay =
                static_cast<int64_t>(arrivalTicks) - static_cast<int64_t>(std::llround(m_clock.Map(senderTicks.front())));

            UpdateDelay(arrivalTicks, oldestDelay, ticksPerMillisecond);

            m_active = true;

            int64_t const maximumDelayTicks = MicrosecondsToTicks(static_cast<uint64_t>(MaximumDelayMilliseconds) * 1000, ticksPerMillisecond);

            releaseTicks.reserve(senderTicks.size());

            for (auto const sender : senderTicks)
            {
                int64_t release = static_cast<int64_t>(std::llround(m_clock.Map(sender))) + m_delayTicks;

                if (release < static_cast<int64_t>(arrivalTicks))
                {
                    release = static_cast<int64_t>(arrivalTicks);
                    m_lateMessageCount++;
                }
                else if (release > static_cast<int64_t>(arrivalTicks) + maximumDelayTicks)
                {
                    release = static_cast<int64_t>(arrivalTicks) + maximumDelayTicks;
                }

                uint64_t result = static_cast<uint64_t>(release);

                if (result < m_lastReleaseTicks)
                {
                    result = m_lastReleaseTicks;
                }

                m_lastReleaseTicks = result;
                releaseTicks.push_back(result);
            }

            return true;
        }

    private:
        void ReleaseAllAtArrival(
            _In_ size_t const count,
            _In_ uint64_t const arrivalTicks,
            _Inout_ std::vector<uint64_t>& releaseTicks)
        {
            uint64_t const release = (std::max)(arrivalTicks, m_lastReleaseTicks);

            m_lastReleaseTicks = release;
            releaseTicks.assign(count, release);
        }

        void UpdateDelay(
            _In_ uint64_t const arrivalTicks,
            _In_ int64_t const packetDelayTicks,
            _In_ uint64_t const ticksPerMillisecond)
        {
            int64_t const minimum = MicrosecondsToTicks(MinimumDelayMicroseconds, ticksPerMillisecond);
            int64_t const maximum = MicrosecondsToTicks(static_cast<uint64_t>(MaximumDelayMilliseconds) * 1000, ticksPerMillisecond);
            int64_t const guard = MicrosecondsToTicks(DelayGuardMicroseconds, ticksPerMillisecond);

            // Measuring means a pass over every packet in the window, so it's done now and then,
            // and straight away when this packet didn't fit.
            if (!m_haveTarget ||
                packetDelayTicks + guard > m_delayTicks ||
                arrivalTicks - m_lastMeasureArrivalTicks >= static_cast<uint64_t>(DelayMeasureIntervalMilliseconds) * ticksPerMillisecond)
            {
                auto const& clock = m_clock;

                m_targetTicks = (std::clamp)(
                    m_delays.CoveringDelayTicks([&clock](int64_t const sender) { return clock.Map(sender); }) + guard,
                    minimum,
                    maximum);

                m_haveTarget = true;
                m_lastMeasureArrivalTicks = arrivalTicks;
            }

            if (m_delayTicks == 0)
            {
                uint32_t const interval = m_connectionIntervalMicroseconds != 0 ?
                    m_connectionIntervalMicroseconds : DefaultConnectionIntervalMicroseconds;

                int64_t const initial = (std::clamp)(
                    MicrosecondsToTicks(static_cast<uint64_t>(interval) + static_cast<uint64_t>(InitialDelayMarginMilliseconds) * 1000, ticksPerMillisecond),
                    minimum,
                    maximum);

                m_delayTicks = (std::max)(m_targetTicks, initial);
                m_lastShrinkArrivalTicks = arrivalTicks;

                return;
            }

            if (m_targetTicks > m_delayTicks)
            {
                // Messages are already arriving late, so waiting to grow only makes more of them late.
                m_delayTicks = m_targetTicks;

                return;
            }

            if (m_targetTicks < m_delayTicks &&
                m_delays.Count() >= MinimumDelaySamplesBeforeShrinking &&
                arrivalTicks - m_lastShrinkArrivalTicks >= static_cast<uint64_t>(ShrinkIntervalMilliseconds) * ticksPerMillisecond)
            {
                int64_t const step = (std::clamp)(
                    (m_delayTicks - m_targetTicks) / ShrinkDivisor,
                    MicrosecondsToTicks(SmallestShrinkStepMicroseconds, ticksPerMillisecond),
                    MicrosecondsToTicks(LargestShrinkStepMicroseconds, ticksPerMillisecond));

                m_delayTicks = (std::max)(m_targetTicks, m_delayTicks - step);
                m_lastShrinkArrivalTicks = arrivalTicks;
            }
        }

        ClockEstimator m_clock{ };
        DelayTracker m_delays{ };

        int64_t m_delayTicks{ 0 };
        int64_t m_targetTicks{ 0 };
        bool m_haveTarget{ false };
        uint64_t m_lastMeasureArrivalTicks{ 0 };
        bool m_active{ false };
        uint64_t m_lastReleaseTicks{ 0 };
        uint64_t m_lastShrinkArrivalTicks{ 0 };
        uint32_t m_connectionIntervalMicroseconds{ 0 };
        uint64_t m_lateMessageCount{ 0 };
    };
}

#endif
