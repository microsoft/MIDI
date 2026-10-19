// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Turns kernel trace events into Results. Fed one event at a time from a trace's event
// callback, live or from a file, on one thread.
//
// Two passes over each event:
//
// 1. As it arrives: DPC and interrupt durations, the driver framework's own events, and profile
//    samples. These only need the events of one processor in order, which a trace always gives.
//
// 2. In time order across all processors: context switches, threads made ready, and the DPCs and
//    interrupts again, to see which audio threads were kept waiting and what ran instead. A live
//    trace delivers each processor's events in batches, so these wait in a queue per processor
//    until every processor has caught up past them. A file is already in time order.

#include "LatencyModel.h"

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace miditroubleshooter::latency
{
    // {70da8294-a20e-5f5f-4388-18fdeaf90bd2}, "Microsoft.Windows.Midi2.LatencyCheck". The probe
    // names its thread with this provider, so a saved trace can be checked again later.
    inline constexpr GUID LatencyCheckProviderGuid{ 0x70da8294, 0xa20e, 0x5f5f, { 0x43, 0x88, 0x18, 0xfd, 0xea, 0xf9, 0x0b, 0xd2 } };

    struct AnalyzerSettings
    {
        // ticks per second of the trace's clock
        uint64_t Frequency{ 0 };

        uint32_t ProcessorCount{ 0 };
        uint32_t PointerSize{ 8 };
        uint32_t ClockType{ 1 };

        // A file: ProcessTrace already delivers its events in time order.
        bool TimeOrdered{ false };

        // the check's own test thread, when this is a live check
        uint32_t ProbeProcessId{ 0 };
        uint32_t ProbeThreadId{ 0 };
    };

    class Analyzer
    {
    public:
        explicit Analyzer(_In_ AnalyzerSettings const& settings);

        Analyzer(Analyzer const&) = delete;
        Analyzer& operator=(Analyzer const&) = delete;

        // From the trace's event callback. Never throws: a malformed event is skipped.
        void OnEvent(_In_ EVENT_RECORD const& record) noexcept;

        // After the last event: processes whatever is still waiting and builds the results.
        void Finish() noexcept;

        // Valid after Finish. Moves the results out.
        Results TakeResults() noexcept;

        // A copy of how the check is going. Safe from any thread.
        LiveStatus Snapshot() const;

        // Lost event counts, which the session reports when it stops.
        void SetLostCounts(_In_ uint64_t const eventsLost, _In_ uint64_t const buffersLost, _In_ uint64_t const realTimeBuffersLost) noexcept;

    private:
        enum class OrderedKind : uint8_t
        {
            ContextSwitch,
            ReadyThread,
            Interval,
            HardFault,

            // the test thread woke late; Start is when it should have woken
            ProbeLateWake,

            // a profile sample; Address is the code that was running
            Sample
        };

        // An event for the time-ordered pass, small enough to queue by the hundred thousand.
        struct OrderedEvent
        {
            uint64_t Time{ 0 };

            // DPC, interrupt or hard fault start
            uint64_t Start{ 0 };

            // sample: the instruction pointer
            uint64_t Address{ 0 };

            uint32_t Processor{ 0 };

            // context switch: the new thread. ready: the thread made ready. hard fault: the thread.
            // sample: the thread that was running.
            uint32_t ThreadId{ 0 };

            // context switch: the old thread
            uint32_t OtherThreadId{ 0 };

            // index into m_routines
            int32_t Routine{ -1 };

            int8_t NewPriority{ 0 };
            int8_t OldPriority{ 0 };
            uint8_t IdleState{ 0 };
            bool IsIsr{ false };

            OrderedKind Kind{ OrderedKind::ContextSwitch };
        };

        struct Interval
        {
            uint64_t Start{ 0 };
            uint64_t End{ 0 };
            int32_t Routine{ -1 };
            bool IsIsr{ false };
        };

        struct ThreadRun
        {
            uint64_t Start{ 0 };
            uint64_t End{ 0 };
            uint32_t ThreadId{ 0 };
            int32_t Priority{ 0 };

            // for the idle thread, the C-state the processor was in
            uint8_t IdleState{ 0 };
        };

        // A fixed-size history, newest last. It grows to its full size as items arrive, so a
        // damaged trace that logs one event on each of thousands of processors costs little.
        template <typename T>
        class Ring
        {
        public:
            explicit Ring(_In_ size_t const capacity) : m_capacity{ capacity } {}

            void Push(_In_ T const& item)
            {
                if (m_items.size() < m_capacity)
                {
                    if (m_items.size() == m_items.capacity())
                    {
                        m_items.reserve((std::min)(m_capacity, (std::max)(InitialCapacity, m_items.size() * 2)));
                    }

                    m_items.push_back(item);
                    m_next = m_items.size() % m_capacity;
                    m_count = m_items.size();
                    return;
                }

                m_items[m_next] = item;
                m_next = (m_next + 1) % m_items.size();
            }

            size_t Count() const noexcept { return m_count; }

            // 0 is the newest
            T const& FromNewest(_In_ size_t const index) const noexcept
            {
                return m_items[(m_next + m_items.size() - 1 - index) % m_items.size()];
            }

        private:
            static constexpr size_t InitialCapacity{ 64 };

            size_t m_capacity{ 0 };
            std::vector<T> m_items;
            size_t m_next{ 0 };
            size_t m_count{ 0 };
        };

        struct Sample
        {
            uint64_t Time{ 0 };
            uint64_t InstructionPointer{ 0 };
            bool Claimed{ false };
        };

        // the driver framework called a driver's own routine
        struct FrameworkCall
        {
            uint64_t Time{ 0 };
            uint64_t Routine{ 0 };
            bool IsIsr{ false };
        };

        // a profile sample, for the time-ordered pass
        struct TimedSample
        {
            uint64_t Time{ 0 };
            uint64_t InstructionPointer{ 0 };
            uint32_t ThreadId{ 0 };
        };

        struct ProcessorState
        {
            ProcessorState();

            // first pass
            std::deque<Sample> RecentSamples{};
            std::vector<FrameworkCall> FrameworkCalls{};
            uint64_t LatestTime{ 0 };

            // waiting for the time-ordered pass
            std::deque<OrderedEvent> Queue{};

            // time-ordered pass
            bool RunningKnown{ false };
            uint32_t RunningThread{ 0 };
            int32_t RunningPriority{ 0 };
            uint64_t RunningSince{ 0 };
            Ring<Interval> Intervals;
            Ring<ThreadRun> Runs;
            Ring<TimedSample> OrderedSamples;
        };

        struct ThreadState
        {
            uint32_t ProcessId{ 0 };
            bool ProcessKnown{ false };

            int32_t HighestPriority{ 0 };

            // how many times it started running, and how many of those at real-time priority
            uint32_t Runs{ 0 };
            uint32_t RealTimeRuns{ 0 };

            // index into m_results.AudioThreads and m_candidates, -1 while it isn't a candidate
            int32_t AudioIndex{ -1 };

            bool ReadyPending{ false };
            uint64_t ReadyTime{ 0 };
            uint32_t ReadyProcessor{ 0 };

            std::wstring Name{};
        };

        struct RoutineKey
        {
            uint64_t Routine{ 0 };
            uint64_t ClientRoutine{ 0 };
            InterruptKind Kind{ InterruptKind::Dpc };

            bool operator==(RoutineKey const& other) const noexcept = default;
        };

        struct RoutineKeyHash
        {
            size_t operator()(_In_ RoutineKey const& key) const noexcept
            {
                return std::hash<uint64_t>{}(key.Routine * 31 + key.ClientRoutine * 7 + static_cast<uint64_t>(key.Kind));
            }
        };

        // late moments each routine was the biggest cause of
        struct RoutineBlame
        {
            uint64_t Count{ 0 };
            uint64_t WorstTicks{ 0 };
        };

        // A thread seen at real-time priority, which may or may not turn out to be an audio
        // thread. What held it up is kept per thread, because whether it counts is only decided
        // once the check has seen how it usually ran.
        struct CandidateState
        {
            std::array<uint32_t, 32> PriorityRuns{};
            std::vector<LateMoment> LongestWaits{};
            std::unordered_map<int32_t, RoutineBlame> Blame{};

            // the same for threads that held a processor, by the module they were running
            std::unordered_map<int32_t, RoutineBlame> ModuleBlame{};
        };

        // first pass. These can run out of memory, which OnEvent catches.
        void OnInterruptEvent(_In_ EVENT_RECORD const& record, _In_ uint32_t const processor, _In_ InterruptKind const kind);
        void OnFrameworkEvent(_In_ EVENT_RECORD const& record, _In_ uint32_t const processor, _In_ bool const isIsr);
        void OnSample(_In_ EVENT_RECORD const& record, _In_ uint32_t const processor);
        void OnImage(_In_ EVENT_RECORD const& record) noexcept;
        void OnProcess(_In_ EVENT_RECORD const& record) noexcept;
        void OnThread(_In_ EVENT_RECORD const& record) noexcept;
        void OnThreadName(_In_ EVENT_RECORD const& record) noexcept;
        void OnProbeEvent(_In_ EVENT_RECORD const& record) noexcept;

        // From a catch block: an event that couldn't be handled.
        void SkipEvent() noexcept;

        int32_t FindModule(_In_ uint64_t const address) noexcept;
        int32_t RoutineIndex(_In_ RoutineKey const& key);
        ProcessorState& Processor(_In_ uint32_t const index);
        ThreadState& Thread(_In_ uint32_t const threadId);
        static void AddSampledModule(_Inout_ RoutineStats& routine, _In_ int32_t const module);
        std::wstring RoutineDisplayName(_In_ int32_t const index);

        // time-ordered pass
        void Order(_In_ OrderedEvent const& event);
        void DrainOrdered(_In_ bool const everything);
        void OnOrdered(_In_ OrderedEvent const& event) noexcept;
        void OnContextSwitch(_In_ OrderedEvent const& event);
        void OnInterval(_In_ OrderedEvent const& event);
        void OnHardFault(_In_ OrderedEvent const& event);
        void OnProbeLateWake(_In_ OrderedEvent const& event);

        // what kept a processor from running a thread between two times, biggest first. With
        // interruptsOnly, only the DPCs and interrupts, which are all that can hold up a timer.
        std::vector<WaitCause> Blame(
            _In_ uint32_t const processor,
            _In_ uint64_t const from,
            _In_ uint64_t const to,
            _In_ uint32_t const waitingThread,
            _In_ bool const interruptsOnly = false) const;

        // A timer that went off late with no DPC or interrupt to blame: what the processor that
        // runs it was doing. A RaisedIrql cause when one thread held it running kernel code, an
        // Unknown cause when that thread took no profile samples, and nothing for timer slack.
        std::optional<WaitCause> FindStall(
            _In_ uint32_t const processor,
            _In_ uint64_t const from,
            _In_ uint64_t const to);

        void RecordMoment(_In_ int32_t const audioIndex, _In_ LateMoment&& moment);

        AudioThreadStats* AudioThread(_In_ uint32_t const threadId, _In_ ThreadState& state, _In_ int32_t const priority);

        // An audio thread runs at real-time priority nearly every time it runs, in a process
        // that isn't part of Windows' own user interface.
        bool CountsAsAudio(_In_ AudioThreadStats const& audio) const;

        void UpdateSnapshot(_In_ bool const force) noexcept;

        // Finish
        void ClassifyAudioThreads();
        void ChargeRoutinesToDrivers();
        void BuildDrivers();

        AnalyzerSettings m_settings{};
        Results m_results{};

        std::vector<ProcessorState> m_processors{};
        std::unordered_map<uint32_t, ThreadState> m_threads{};
        std::unordered_map<uint32_t, std::wstring> m_processNames{};

        // what Thread hands out once m_threads is as big as it may get
        ThreadState m_overflowThread{};

        // modules by base address, to see the ones the rundown lists again
        std::unordered_map<uint64_t, int32_t> m_moduleBases{};

        // the entry for each kind of routine once m_results.Routines is as big as it may get
        std::array<int32_t, 4> m_overflowRoutines{ -1, -1, -1, -1 };

        // how many processor time entries the routines hold between them
        size_t m_processorTickEntries{ 0 };

        // modules sorted by base address, rebuilt when one is added
        std::vector<int32_t> m_moduleOrder{};
        bool m_moduleOrderStale{ false };

        std::unordered_map<RoutineKey, int32_t, RoutineKeyHash> m_routineIndexes{};

        // Late moments of the audio threads, by routine. Filled at Finish from m_candidates.
        std::unordered_map<int32_t, RoutineBlame> m_routineBlame{};

        // the same for threads that held a processor, by the module they were running
        std::unordered_map<int32_t, RoutineBlame> m_moduleBlame{};

        // how long threads held a processor where timers wait, by the module they were running
        std::unordered_map<int32_t, DurationStats> m_raisedIrql{};

        // one for each entry in m_results.AudioThreads until Finish sorts them out
        std::vector<CandidateState> m_candidates{};

        // samples taken before their module was known, by routine, tried again at Finish
        std::unordered_map<int32_t, std::vector<uint64_t>> m_unresolvedSamples{};

        // the last late moment on each processor, so one incident seen twice is counted once
        struct Incident
        {
            bool Valid{ false };
            bool Counted{ false };
            MomentKind Kind{ MomentKind::WaitedToRun };
            uint32_t ThreadId{ 0 };
            uint64_t Start{ 0 };
            uint64_t End{ 0 };
        };

        std::vector<Incident> m_incidents{};

        // for the live status
        uint64_t m_longestDpcTicks{ 0 };
        int32_t m_longestDpcRoutine{ -1 };
        uint64_t m_longestIsrTicks{ 0 };
        int32_t m_longestIsrRoutine{ -1 };

        // the newest time every processor has passed, and the newest time processed in order
        uint64_t m_latestTime{ 0 };
        uint64_t m_firstTime{ 0 };
        uint64_t m_orderedTime{ 0 };
        uint64_t m_eventsSinceDrain{ 0 };

        mutable std::mutex m_snapshotLock{};
        LiveStatus m_snapshot{};
        uint64_t m_snapshotTime{ 0 };

        bool m_finished{ false };
    };
}
