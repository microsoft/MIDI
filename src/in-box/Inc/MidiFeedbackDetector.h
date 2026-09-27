// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <new>

#include "ump_helpers.h"

namespace WindowsMidiServicesInternal
{
    enum class MidiFeedbackTest : uint8_t
    {
        None = 0,

        // the same messages keep coming back
        Repeat = 1,

        // more messages than any sender produces on purpose
        Runaway = 2,
    };

    enum class MidiFeedbackPhase : uint8_t
    {
        Watching = 0,
        FirstPause = 1,
        Releasing = 2,
        ReseedWatch = 3,
        SecondPause = 4,
        Tripped = 5,
    };

    enum class MidiFeedbackSendAction : uint8_t
    {
        Deliver = 0,
        Hold = 1,
        Drop = 2,
    };

    enum class MidiFeedbackTimerAction : uint8_t
    {
        None = 0,

        // deliver everything held, in order, then call OnReleaseComplete
        Release = 1,

        // discard everything held and deliver nothing more until Reset
        Trip = 2,
    };

    struct MidiFeedbackTripInfo
    {
        MidiFeedbackTest Test{ MidiFeedbackTest::None };

        // all messages, in the last full measurement period before the first pause
        uint32_t MessagesPerSecond{ 0 };
        uint32_t RepeatPercent{ 0 };

        uint32_t LapMicroseconds{ 0 };
        uint32_t PauseMicroseconds{ 0 };

        // from the second pause, which is the one that decided
        uint32_t ExpectedArrivals{ 0 };
        uint32_t ObservedArrivals{ 0 };
    };

    // Decides whether one direction of a loopback is feeding back into itself.
    //
    // Counting only decides WHEN to test. The test itself is about cause and effect: a loop only
    // keeps going because the loopback keeps delivering, while a real sender keeps sending no
    // matter what happens downstream. So the owner holds everything for a moment. If the traffic
    // stops, it is released; if it then starts again, it is held again; and only if it stops a
    // second time is it treated as a loop. Anything held is released in order when it is not a
    // loop, so a false alarm delays messages and never loses them.
    //
    // Not thread safe. The owner serializes every call, supplies the clock, keeps the messages
    // that are held and runs a timer for NextDeadlineMicroseconds(). Nothing here allocates except
    // the fingerprint table, once, the first time a direction gets busy enough to need it.
    class MidiFeedbackDetector
    {
    public:
        static constexpr uint64_t BucketMicroseconds{ 100'000 };

        // below this many analyzed messages a second, nothing is fingerprinted
        static constexpr uint32_t ArmMessagesPerBucket{ 200 };

        static constexpr uint32_t RepeatPercentThreshold{ 90 };
        static constexpr uint32_t RepeatBucketsToPause{ 5 };
        static constexpr uint64_t RepeatWindowMicroseconds{ 50'000 };

        // counts every packet, including real time and each part of a system exclusive message
        static constexpr uint32_t RunawayMessagesPerBucket{ 3'000 };
        static constexpr uint32_t RunawayBucketsToPause{ 10 };

        // Client buffers are one page per direction, so a saturated loop has at most a few
        // thousand messages still on their way back when the pause starts.
        static constexpr uint64_t InFlightMessageBound{ 2'048 };

        static constexpr uint64_t DefaultLapMicroseconds{ 2'000 };
        static constexpr uint64_t MinimumDrainMicroseconds{ 1'000 };
        static constexpr uint64_t MaximumDrainMicroseconds{ 100'000 };
        static constexpr uint64_t MinimumObserveMicroseconds{ 15'000 };
        static constexpr uint64_t MaximumObserveMicroseconds{ 50'000 };
        static constexpr uint64_t MaximumPauseMicroseconds{ 150'000 };

        // fewer expected arrivals than this and a quiet pause proves nothing
        static constexpr uint32_t MinimumExpectedArrivals{ 20 };
        static constexpr uint32_t CollapsedPercent{ 10 };
        static constexpr uint32_t ResumedPercent{ 25 };

        static constexpr uint64_t FirstBackoffMicroseconds{ 60'000'000 };
        static constexpr uint64_t MaximumBackoffMicroseconds{ 600'000'000 };

        static constexpr uint32_t TableSlotCount{ 1'024 };
        static constexpr uint32_t LapBinCount{ 20 };

        MidiFeedbackDetector() = default;

        MidiFeedbackDetector(_In_ MidiFeedbackDetector const&) = delete;
        MidiFeedbackDetector& operator=(_In_ MidiFeedbackDetector const&) = delete;

        MidiFeedbackPhase Phase() const noexcept { return m_phase; }
        MidiFeedbackTest ActiveTest() const noexcept { return m_test; }
        MidiFeedbackTripInfo const& TripInfo() const noexcept { return m_tripInfo; }

        // Zero when no timer is needed.
        uint64_t NextDeadlineMicroseconds() const noexcept { return m_deadlineUs; }

        // Goes up by one each time a test ends with "not a loop", so the owner can report it.
        uint32_t NotALoopCount() const noexcept { return m_notALoopCount; }

        // The owner must hold the message for Hold, discard it for Drop and deliver it otherwise.
        MidiFeedbackSendAction OnSend(
            _In_reads_(wordCount) uint32_t const* const words,
            _In_ uint32_t const wordCount,
            _In_ uint64_t const nowUs) noexcept
        {
            if (m_phase == MidiFeedbackPhase::Tripped)
            {
                return MidiFeedbackSendAction::Drop;
            }

            if (m_phase == MidiFeedbackPhase::ReseedWatch && m_deadlineUs != 0 && nowUs >= m_deadlineUs)
            {
                // A late timer must not let a restarted loop run on unchecked.
                EvaluateReseedWatch(nowUs);
            }

            if (m_phase == MidiFeedbackPhase::Watching)
            {
                AdvanceBuckets(nowUs);
            }

            if (words != nullptr)
            {
                for (uint32_t index = 0; index < wordCount; )
                {
                    auto length = static_cast<uint32_t>(GetUmpLengthInMidiWordsFromFirstWord(words[index]));

                    // The caller validated the buffer, but a short tail must never walk off the end.
                    if (length == 0 || index + length > wordCount)
                    {
                        length = wordCount - index;
                    }

                    ProcessMessage(words + index, length, nowUs);

                    index += length;
                }
            }

            switch (m_phase)
            {
            case MidiFeedbackPhase::FirstPause:
            case MidiFeedbackPhase::SecondPause:
            case MidiFeedbackPhase::Releasing:
                // Anything arriving while a release is still going out has to queue behind it.
                return MidiFeedbackSendAction::Hold;

            default:
                return MidiFeedbackSendAction::Deliver;
            }
        }

        MidiFeedbackTimerAction OnTimer(_In_ uint64_t const nowUs) noexcept
        {
            if (m_deadlineUs == 0 || nowUs < m_deadlineUs)
            {
                return MidiFeedbackTimerAction::None;
            }

            switch (m_phase)
            {
            case MidiFeedbackPhase::FirstPause:
            case MidiFeedbackPhase::SecondPause:
                return EvaluatePause(nowUs);

            case MidiFeedbackPhase::ReseedWatch:
                EvaluateReseedWatch(nowUs);
                return MidiFeedbackTimerAction::None;

            default:
                return MidiFeedbackTimerAction::None;
            }
        }

        // The hold queue is full, which means the sender kept going: not a loop. Returns true when
        // the owner must now release what it holds.
        bool OnHoldQueueFull() noexcept
        {
            if (m_phase != MidiFeedbackPhase::FirstPause && m_phase != MidiFeedbackPhase::SecondPause)
            {
                return false;
            }

            m_afterRelease = AfterRelease::Backoff;
            m_phase = MidiFeedbackPhase::Releasing;
            m_deadlineUs = 0;

            return true;
        }

        void OnReleaseComplete(_In_ uint64_t const nowUs) noexcept
        {
            if (m_phase != MidiFeedbackPhase::Releasing)
            {
                return;
            }

            switch (m_afterRelease)
            {
            case AfterRelease::ReseedWatch:
                m_phase = MidiFeedbackPhase::ReseedWatch;
                m_observeStartUs = nowUs + m_drainUs;
                m_deadlineUs = m_observeStartUs + m_observeUs;
                m_observed = 0;
                break;

            case AfterRelease::Watching:
                ReturnToWatching(nowUs);
                break;

            default:
                EnterBackoff(nowUs);
                break;
            }
        }

        // Ends any test in progress without a verdict. Returns true when the owner holds messages
        // it must now release, in order, before calling OnReleaseComplete.
        bool EndTestAndRelease() noexcept
        {
            if (m_phase != MidiFeedbackPhase::FirstPause && m_phase != MidiFeedbackPhase::SecondPause)
            {
                return false;
            }

            m_afterRelease = AfterRelease::Watching;
            m_phase = MidiFeedbackPhase::Releasing;
            m_deadlineUs = 0;

            return true;
        }

        // Back to a clean start, as if the loopback had just been created. The fingerprint table
        // is kept so a busy direction does not allocate again.
        void Reset() noexcept
        {
            m_phase = MidiFeedbackPhase::Watching;
            m_test = MidiFeedbackTest::None;
            m_afterRelease = AfterRelease::Backoff;
            m_deadlineUs = 0;
            m_backoffUntilUs = 0;
            m_nextBackoffUs = FirstBackoffMicroseconds;
            m_tripInfo = {};
            m_bucketStarted = false;
            ClearBucketCounters();
            ResetRepeatStreak();
            m_runawayStreak = 0;
            m_armed = false;
            ClearSystemExclusiveState();
        }

    private:
        enum class AfterRelease : uint8_t
        {
            Backoff = 0,
            ReseedWatch = 1,
            Watching = 2,
        };

        struct TableSlot
        {
            uint64_t Fingerprint{ 0 };
            uint64_t LastSeenUs{ 0 };
            uint32_t HotEpoch{ 0 };
        };

        enum class MessageKind : uint8_t
        {
            // counted, never fingerprinted: utility messages carry clock values that change
            CountOnly = 0,
            Single = 1,
            SystemExclusiveStart = 2,
            SystemExclusiveContinue = 3,
            SystemExclusiveEnd = 4,
            SystemExclusiveComplete = 5,
        };

        static constexpr uint64_t FingerprintSeed{ 0xCBF29CE484222325ULL };

        static uint64_t MixWord(_In_ uint64_t hash, _In_ uint32_t const word) noexcept
        {
            hash ^= word;
            hash *= 0x9E3779B97F4A7C15ULL;
            hash ^= hash >> 29;

            return hash;
        }

        static uint64_t FinishFingerprint(_In_ uint64_t hash) noexcept
        {
            hash ^= hash >> 32;
            hash *= 0xD6E8FEB86659FD93ULL;
            hash ^= hash >> 32;

            // zero marks an empty table slot
            return hash == 0 ? 1 : hash;
        }

        static uint64_t HashWords(
            _In_ uint64_t hash,
            _In_reads_(length) uint32_t const* const words,
            _In_ uint32_t const length) noexcept
        {
            for (uint32_t index = 0; index < length; index++)
            {
                hash = MixWord(hash, words[index]);
            }

            return hash;
        }

        static MessageKind ClassifyMessage(_In_ uint32_t const firstWord) noexcept
        {
            auto const messageType = static_cast<uint8_t>(firstWord >> 28);

            if (messageType == 0x0)
            {
                return MessageKind::CountOnly;
            }

            if (messageType == 0x3 || messageType == 0x5)
            {
                switch ((firstWord >> 20) & 0xF)
                {
                case 0x0: return MessageKind::SystemExclusiveComplete;
                case 0x1: return MessageKind::SystemExclusiveStart;
                case 0x2: return MessageKind::SystemExclusiveContinue;
                case 0x3: return MessageKind::SystemExclusiveEnd;
                default:  return MessageKind::Single;
                }
            }

            return MessageKind::Single;
        }

        static bool IsAnalyzed(_In_ MessageKind const kind) noexcept
        {
            return kind == MessageKind::Single ||
                kind == MessageKind::SystemExclusiveEnd ||
                kind == MessageKind::SystemExclusiveComplete;
        }

        static uint32_t PerSecondFromBucket(_In_ uint32_t const count) noexcept
        {
            return static_cast<uint32_t>((static_cast<uint64_t>(count) * 1'000'000) / BucketMicroseconds);
        }

        static uint64_t Clamp(_In_ uint64_t const value, _In_ uint64_t const low, _In_ uint64_t const high) noexcept
        {
            return value < low ? low : (value > high ? high : value);
        }

        bool FingerprintingActive() const noexcept
        {
            switch (m_phase)
            {
            case MidiFeedbackPhase::Watching:
                return m_armed;

            case MidiFeedbackPhase::FirstPause:
            case MidiFeedbackPhase::SecondPause:
            case MidiFeedbackPhase::ReseedWatch:
                return m_test == MidiFeedbackTest::Repeat;

            default:
                return false;
            }
        }

        bool EnsureTable() noexcept
        {
            if (m_table == nullptr)
            {
                m_table.reset(new (std::nothrow) TableSlot[TableSlotCount]{});
            }

            return m_table != nullptr;
        }

        // A whole system exclusive message gets one fingerprint, built across its packets, so a
        // dump that repeats is seen as the same message and one that does not is not.
        bool Fingerprint(
            _In_reads_(length) uint32_t const* const words,
            _In_ uint32_t const length,
            _In_ MessageKind const kind,
            _Out_ uint64_t& fingerprint) noexcept
        {
            fingerprint = 0;

            auto const isData128 = (words[0] >> 28) == 0x5;
            auto const group = static_cast<uint8_t>((words[0] >> 24) & 0xF);

            auto& hash = isData128 ? m_systemExclusive8Hash[group] : m_systemExclusive7Hash[group];
            auto& active = isData128 ? m_systemExclusive8Active[group] : m_systemExclusive7Active[group];

            switch (kind)
            {
            case MessageKind::Single:
            case MessageKind::SystemExclusiveComplete:
                fingerprint = FinishFingerprint(HashWords(FingerprintSeed, words, length));
                return true;

            case MessageKind::SystemExclusiveStart:
                hash = HashWords(FingerprintSeed, words, length);
                active = true;
                return false;

            case MessageKind::SystemExclusiveContinue:
                if (active)
                {
                    hash = HashWords(hash, words, length);
                }
                return false;

            case MessageKind::SystemExclusiveEnd:
                fingerprint = FinishFingerprint(HashWords(active ? hash : FingerprintSeed, words, length));
                active = false;
                return true;

            default:
                return false;
            }
        }

        void ClearSystemExclusiveState() noexcept
        {
            for (uint32_t group = 0; group < 16; group++)
            {
                m_systemExclusive7Active[group] = false;
                m_systemExclusive8Active[group] = false;
            }
        }

        bool IsHot(_In_ uint64_t const fingerprint) const noexcept
        {
            if (m_table == nullptr)
            {
                return false;
            }

            auto const& slot = m_table[fingerprint & (TableSlotCount - 1)];

            return slot.Fingerprint == fingerprint && slot.HotEpoch == m_hotEpoch;
        }

        void RecordFingerprint(_In_ uint64_t const fingerprint, _In_ uint64_t const nowUs) noexcept
        {
            auto& slot = m_table[fingerprint & (TableSlotCount - 1)];

            if (slot.Fingerprint == fingerprint)
            {
                auto const gapUs = nowUs >= slot.LastSeenUs ? nowUs - slot.LastSeenUs : 0;

                if (gapUs <= RepeatWindowMicroseconds)
                {
                    m_bucketRepeats++;
                    slot.HotEpoch = m_hotEpoch;
                    AddLapSample(gapUs);
                }
            }
            else
            {
                slot.Fingerprint = fingerprint;
                slot.HotEpoch = 0;
            }

            slot.LastSeenUs = nowUs;
        }

        void AddLapSample(_In_ uint64_t gapUs) noexcept
        {
            uint32_t bin{ 0 };

            while (gapUs > 1 && bin < LapBinCount - 1)
            {
                gapUs >>= 1;
                bin++;
            }

            m_lapBins[bin]++;
            m_lapSamples++;
        }

        // In a loop every message comes around once per lap, so the typical gap between two
        // sightings of the same message is the time the loop takes to go around.
        uint64_t MedianLapMicroseconds() const noexcept
        {
            if (m_lapSamples < 8)
            {
                return DefaultLapMicroseconds;
            }

            uint32_t cumulative{ 0 };
            auto const half = (m_lapSamples + 1) / 2;

            for (uint32_t bin = 0; bin < LapBinCount; bin++)
            {
                cumulative += m_lapBins[bin];

                if (cumulative >= half)
                {
                    auto const low = 1ULL << bin;
                    return low + low / 2;
                }
            }

            return DefaultLapMicroseconds;
        }

        void ClearBucketCounters() noexcept
        {
            m_bucketAll = 0;
            m_bucketAnalyzed = 0;
            m_bucketRepeats = 0;
        }

        void ResetRepeatStreak() noexcept
        {
            m_repeatStreak = 0;

            // every mark from the streak that just ended goes stale at once
            m_hotEpoch++;

            if (m_hotEpoch == 0)
            {
                m_hotEpoch = 1;
            }

            for (uint32_t bin = 0; bin < LapBinCount; bin++)
            {
                m_lapBins[bin] = 0;
            }

            m_lapSamples = 0;
        }

        void AdvanceBuckets(_In_ uint64_t const nowUs) noexcept
        {
            if (!m_bucketStarted)
            {
                m_bucketStarted = true;
                m_bucketStartUs = nowUs;
                return;
            }

            auto const elapsedUs = nowUs >= m_bucketStartUs ? nowUs - m_bucketStartUs : 0;

            if (elapsedUs < BucketMicroseconds)
            {
                return;
            }

            CloseBucket();

            if (elapsedUs >= 2 * BucketMicroseconds)
            {
                // A silent stretch means whatever was happening before has stopped.
                ResetRepeatStreak();
                m_runawayStreak = 0;
                m_armed = false;
            }

            m_bucketStartUs = nowUs;

            if (nowUs < m_backoffUntilUs)
            {
                return;
            }

            if (m_repeatStreak >= RepeatBucketsToPause)
            {
                StartPause(MidiFeedbackTest::Repeat, nowUs);
            }
            else if (m_runawayStreak >= RunawayBucketsToPause)
            {
                StartPause(MidiFeedbackTest::Runaway, nowUs);
            }
        }

        void CloseBucket() noexcept
        {
            bool const suspicious =
                m_armed &&
                m_bucketAnalyzed >= ArmMessagesPerBucket &&
                static_cast<uint64_t>(m_bucketRepeats) * 100 >= static_cast<uint64_t>(m_bucketAnalyzed) * RepeatPercentThreshold;

            if (suspicious)
            {
                m_repeatStreak++;
            }
            else if (m_repeatStreak != 0 || m_lapSamples != 0)
            {
                ResetRepeatStreak();
            }

            m_runawayStreak = m_bucketAll >= RunawayMessagesPerBucket ? m_runawayStreak + 1 : 0;

            m_lastBucketAll = m_bucketAll;
            m_lastBucketAnalyzed = m_bucketAnalyzed;
            m_lastBucketRepeats = m_bucketRepeats;

            auto const busy = m_bucketAnalyzed >= ArmMessagesPerBucket || m_bucketAll >= RunawayMessagesPerBucket;

            // No table means no repeat test. The runaway test still works on counts alone.
            m_armed = busy && EnsureTable();

            ClearBucketCounters();
        }

        void StartPause(_In_ MidiFeedbackTest const test, _In_ uint64_t const nowUs) noexcept
        {
            m_test = test;
            m_lapUs = MedianLapMicroseconds();

            m_ratePerSecond = test == MidiFeedbackTest::Repeat ?
                PerSecondFromBucket(m_lastBucketRepeats) :
                PerSecondFromBucket(m_lastBucketAll);

            auto const allPerSecond = PerSecondFromBucket(m_lastBucketAll);

            // A saturated loop still has full client buffers draining back after delivery stops,
            // however short its lap looks. Nothing arriving before they empty counts.
            uint64_t bufferDrainUs{ 0 };

            if (allPerSecond >= PerSecondFromBucket(RunawayMessagesPerBucket) && allPerSecond > 0)
            {
                bufferDrainUs = (InFlightMessageBound * 1'000'000) / allPerSecond;
            }

            m_drainUs = Clamp(m_lapUs > bufferDrainUs ? m_lapUs : bufferDrainUs, MinimumDrainMicroseconds, MaximumDrainMicroseconds);
            m_observeUs = Clamp(3 * m_lapUs, MinimumObserveMicroseconds, MaximumObserveMicroseconds);

            if (m_drainUs + m_observeUs > MaximumPauseMicroseconds)
            {
                m_observeUs = MaximumPauseMicroseconds - m_drainUs;
            }

            m_tripInfo = {};
            m_tripInfo.Test = test;
            m_tripInfo.MessagesPerSecond = allPerSecond;
            m_tripInfo.RepeatPercent = m_lastBucketAnalyzed == 0 ? 0 :
                static_cast<uint32_t>((static_cast<uint64_t>(m_lastBucketRepeats) * 100) / m_lastBucketAnalyzed);
            m_tripInfo.LapMicroseconds = static_cast<uint32_t>(m_lapUs);
            m_tripInfo.PauseMicroseconds = static_cast<uint32_t>(m_drainUs + m_observeUs);

            BeginPause(MidiFeedbackPhase::FirstPause, nowUs);
        }

        void BeginPause(_In_ MidiFeedbackPhase const phase, _In_ uint64_t const nowUs) noexcept
        {
            m_phase = phase;
            m_observeStartUs = nowUs + m_drainUs;
            m_deadlineUs = m_observeStartUs + m_observeUs;
            m_observed = 0;
        }

        uint64_t ExpectedArrivals(_In_ uint64_t const nowUs) const noexcept
        {
            auto const observedUs = nowUs > m_observeStartUs ? nowUs - m_observeStartUs : 0;

            return (static_cast<uint64_t>(m_ratePerSecond) * observedUs) / 1'000'000;
        }

        MidiFeedbackTimerAction EvaluatePause(_In_ uint64_t const nowUs) noexcept
        {
            auto const expected = ExpectedArrivals(nowUs);

            bool const collapsed =
                expected >= MinimumExpectedArrivals &&
                static_cast<uint64_t>(m_observed) * 100 < expected * CollapsedPercent;

            m_deadlineUs = 0;

            if (m_phase == MidiFeedbackPhase::SecondPause && collapsed)
            {
                m_tripInfo.ExpectedArrivals = static_cast<uint32_t>(expected > UINT32_MAX ? UINT32_MAX : expected);
                m_tripInfo.ObservedArrivals = m_observed;

                m_phase = MidiFeedbackPhase::Tripped;

                return MidiFeedbackTimerAction::Trip;
            }

            m_afterRelease = (m_phase == MidiFeedbackPhase::FirstPause && collapsed) ?
                AfterRelease::ReseedWatch :
                AfterRelease::Backoff;

            m_phase = MidiFeedbackPhase::Releasing;

            return MidiFeedbackTimerAction::Release;
        }

        // A loop that was paused and released picks up again by itself. A sender that happened
        // to go quiet during the first pause has no reason to start again right now.
        void EvaluateReseedWatch(_In_ uint64_t const nowUs) noexcept
        {
            auto const expected = ExpectedArrivals(nowUs);

            bool const resumed =
                expected >= MinimumExpectedArrivals &&
                static_cast<uint64_t>(m_observed) * 100 >= expected * ResumedPercent;

            if (!resumed)
            {
                EnterBackoff(nowUs);
                return;
            }

            auto const observedUs = nowUs > m_observeStartUs ? nowUs - m_observeStartUs : 1;

            // the loop restarted from what was held, so its new rate is the one to expect
            m_ratePerSecond = static_cast<uint32_t>((static_cast<uint64_t>(m_observed) * 1'000'000) / observedUs);

            BeginPause(MidiFeedbackPhase::SecondPause, nowUs);
        }

        void EnterBackoff(_In_ uint64_t const nowUs) noexcept
        {
            m_notALoopCount++;

            m_backoffUntilUs = nowUs + m_nextBackoffUs;
            m_nextBackoffUs = m_nextBackoffUs * 2 > MaximumBackoffMicroseconds ? MaximumBackoffMicroseconds : m_nextBackoffUs * 2;

            ReturnToWatching(nowUs);
        }

        void ReturnToWatching(_In_ uint64_t const nowUs) noexcept
        {
            m_phase = MidiFeedbackPhase::Watching;
            m_test = MidiFeedbackTest::None;
            m_deadlineUs = 0;
            m_bucketStarted = true;
            m_bucketStartUs = nowUs;
            ClearBucketCounters();
            ResetRepeatStreak();
            m_runawayStreak = 0;
            m_armed = false;
        }

        void ProcessMessage(
            _In_reads_(length) uint32_t const* const words,
            _In_ uint32_t const length,
            _In_ uint64_t const nowUs) noexcept
        {
            if (m_bucketAll < UINT32_MAX)
            {
                m_bucketAll++;
            }

            auto const kind = ClassifyMessage(words[0]);
            auto const analyzed = IsAnalyzed(kind);

            if (analyzed && m_bucketAnalyzed < UINT32_MAX)
            {
                m_bucketAnalyzed++;
            }

            uint64_t fingerprint{ 0 };
            bool haveFingerprint{ false };

            if (FingerprintingActive())
            {
                haveFingerprint = Fingerprint(words, length, kind, fingerprint);
            }

            switch (m_phase)
            {
            case MidiFeedbackPhase::Watching:
                if (haveFingerprint && m_armed && m_table != nullptr)
                {
                    RecordFingerprint(fingerprint, nowUs);
                }
                break;

            case MidiFeedbackPhase::FirstPause:
            case MidiFeedbackPhase::SecondPause:
            case MidiFeedbackPhase::ReseedWatch:
                if (nowUs >= m_observeStartUs && m_observed < UINT32_MAX)
                {
                    if (m_test == MidiFeedbackTest::Runaway || (haveFingerprint && IsHot(fingerprint)))
                    {
                        m_observed++;
                    }
                }
                break;

            default:
                break;
            }
        }

        MidiFeedbackPhase m_phase{ MidiFeedbackPhase::Watching };
        MidiFeedbackTest m_test{ MidiFeedbackTest::None };
        AfterRelease m_afterRelease{ AfterRelease::Backoff };

        uint64_t m_deadlineUs{ 0 };
        uint32_t m_notALoopCount{ 0 };
        uint64_t m_backoffUntilUs{ 0 };
        uint64_t m_nextBackoffUs{ FirstBackoffMicroseconds };

        bool m_bucketStarted{ false };
        uint64_t m_bucketStartUs{ 0 };
        uint32_t m_bucketAll{ 0 };
        uint32_t m_bucketAnalyzed{ 0 };
        uint32_t m_bucketRepeats{ 0 };
        uint32_t m_lastBucketAll{ 0 };
        uint32_t m_lastBucketAnalyzed{ 0 };
        uint32_t m_lastBucketRepeats{ 0 };

        uint32_t m_repeatStreak{ 0 };
        uint32_t m_runawayStreak{ 0 };
        bool m_armed{ false };

        uint32_t m_hotEpoch{ 1 };
        uint32_t m_lapBins[LapBinCount]{};
        uint32_t m_lapSamples{ 0 };

        uint64_t m_lapUs{ DefaultLapMicroseconds };
        uint64_t m_drainUs{ MinimumDrainMicroseconds };
        uint64_t m_observeUs{ MinimumObserveMicroseconds };
        uint64_t m_observeStartUs{ 0 };
        uint32_t m_ratePerSecond{ 0 };
        uint32_t m_observed{ 0 };

        MidiFeedbackTripInfo m_tripInfo{};

        uint64_t m_systemExclusive7Hash[16]{};
        uint64_t m_systemExclusive8Hash[16]{};
        bool m_systemExclusive7Active[16]{};
        bool m_systemExclusive8Active[16]{};

        std::unique_ptr<TableSlot[]> m_table{};
    };
}
