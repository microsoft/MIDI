// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyProbe.h"

#include <avrt.h>
#include <TraceLoggingProvider.h>

#include <algorithm>

#pragma comment(lib, "avrt.lib")

// The probe names its thread in the trace through this provider, so a saved trace can be checked
// again later and the test thread is still told apart from the rest. The name and the id match
// LatencyCheckProviderGuid in LatencyAnalyzer.h.
TRACELOGGING_DEFINE_PROVIDER(
    g_latencyCheckProvider,
    "Microsoft.Windows.Midi2.LatencyCheck",
    // {70da8294-a20e-5f5f-4388-18fdeaf90bd2}
    (0x70da8294, 0xa20e, 0x5f5f, 0x43, 0x88, 0x18, 0xfd, 0xea, 0xf9, 0x0b, 0xd2));

namespace miditroubleshooter::latency
{
    namespace
    {
        // Enough for ten minutes at one wake a millisecond. Past that, the median and the 99th
        // percentile come from the first ten minutes, and the rest still count in the totals.
        constexpr size_t MaximumLatenessSamples{ 600'000 };

        constexpr uint32_t MinimumPeriodMicroseconds{ 250 };
        constexpr uint32_t MaximumPeriodMicroseconds{ 100'000 };

        // how long Start waits for the thread to say it's running
        constexpr DWORD StartTimeoutMilliseconds{ 5000 };

        uint64_t Now() noexcept
        {
            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);

            return static_cast<uint64_t>(counter.QuadPart);
        }

        uint64_t Frequency() noexcept
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);

            return static_cast<uint64_t>(frequency.QuadPart);
        }

        class UniqueHandle
        {
        public:
            explicit UniqueHandle(_In_opt_ HANDLE const handle) noexcept : m_handle{ handle } {}
            ~UniqueHandle() { if (m_handle != nullptr) { CloseHandle(m_handle); } }

            UniqueHandle(UniqueHandle const&) = delete;
            UniqueHandle& operator=(UniqueHandle const&) = delete;

            HANDLE Get() const noexcept { return m_handle; }

        private:
            HANDLE m_handle{ nullptr };
        };
    }

    Probe::~Probe()
    {
        Stop();
    }

    _Use_decl_annotations_
    uint32_t Probe::Start(uint32_t const periodMicroseconds) noexcept
    {
        try
        {
            Stop();

            m_results = ProbeResults{};
            m_lateness.clear();
            m_wakes = 0;
            m_threadId = 0;
            m_periodMicroseconds = std::clamp(periodMicroseconds, MinimumPeriodMicroseconds, MaximumPeriodMicroseconds);
            m_results.PeriodMicroseconds = m_periodMicroseconds;

            m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            m_startedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

            if (m_stopEvent == nullptr || m_startedEvent == nullptr)
            {
                m_results.Error = GetLastError();
                Stop();

                return m_results.Error;
            }

            m_providerRegistered = SUCCEEDED(TraceLoggingRegister(g_latencyCheckProvider));

            // all of it now, so the vector never grows while the thread is timing its wakes
            m_lateness.reserve(MaximumLatenessSamples);

            m_thread = std::thread([this]() noexcept { Run(); });

            if (WaitForSingleObject(m_startedEvent, StartTimeoutMilliseconds) != WAIT_OBJECT_0)
            {
                m_results.Error = ERROR_TIMEOUT;
                Stop();

                return ERROR_TIMEOUT;
            }

            if (!m_results.Ran)
            {
                auto const error = m_results.Error != 0 ? m_results.Error : static_cast<uint32_t>(ERROR_GEN_FAILURE);
                Stop();
                m_results.Error = error;

                return error;
            }

            return ERROR_SUCCESS;
        }
        catch (...)
        {
            Stop();
            m_results.Error = ERROR_OUTOFMEMORY;

            return ERROR_OUTOFMEMORY;
        }
    }

    void Probe::Stop() noexcept
    {
        if (m_stopEvent != nullptr)
        {
            SetEvent(m_stopEvent);
        }

        if (m_thread.joinable())
        {
            try
            {
                m_thread.join();
            }
            catch (...)
            {
                // join only throws for a thread that isn't joinable, which was checked
            }

            FinishResults();
        }

        if (m_stopEvent != nullptr)
        {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }

        if (m_startedEvent != nullptr)
        {
            CloseHandle(m_startedEvent);
            m_startedEvent = nullptr;
        }

        if (m_providerRegistered)
        {
            TraceLoggingUnregister(g_latencyCheckProvider);
            m_providerRegistered = false;
        }
    }

    void Probe::FinishResults() noexcept
    {
        m_results.Wakes = m_wakes.load();
        m_results.ThreadId = m_threadId.load();

        if (m_lateness.empty())
        {
            return;
        }

        try
        {
            auto sorted = m_lateness;
            std::sort(sorted.begin(), sorted.end());

            m_results.MedianLatenessMicroseconds = sorted[sorted.size() / 2];
            m_results.Percentile99LatenessMicroseconds = sorted[(std::min)(sorted.size() - 1, (sorted.size() * 99) / 100)];
        }
        catch (...)
        {
            // without the copy, only the longest and the totals are reported
        }
    }

    void Probe::Run() noexcept
    {
        HANDLE mmcss{ nullptr };

        try
        {
            m_threadId = GetCurrentThreadId();

            // Shown as the thread's name in the trace, for anyone who opens it in Windows
            // Performance Analyzer.
            SetThreadDescription(GetCurrentThread(), L"Windows MIDI Services DPC latency check test thread");

            DWORD taskIndex{ 0 };
            mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

            if (mmcss != nullptr)
            {
                m_results.UsedMmcss = true;
            }
            else
            {
                // An app may have used up this process's share of MMCSS threads. The highest
                // ordinary priority still says something, and the report says which it was.
                m_results.MmcssError = GetLastError();
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
            }

            // A high-resolution timer wakes on time without changing the timer resolution for
            // the whole PC, which timeBeginPeriod would.
            UniqueHandle timer{ CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS) };
            m_results.HighResolutionTimer = timer.Get() != nullptr;

            UniqueHandle fallbackTimer{ timer.Get() == nullptr ? CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS) : nullptr };
            auto const timerHandle = timer.Get() != nullptr ? timer.Get() : fallbackTimer.Get();

            if (timerHandle == nullptr)
            {
                m_results.Error = GetLastError();
                SetEvent(m_startedEvent);

                if (mmcss != nullptr)
                {
                    AvRevertMmThreadCharacteristics(mmcss);
                }

                return;
            }

            if (m_providerRegistered)
            {
                TraceLoggingWrite(
                    g_latencyCheckProvider,
                    "ProbeThread",
                    TraceLoggingUInt32(GetCurrentThreadId(), "ProbeThreadId"),
                    TraceLoggingUInt32(m_periodMicroseconds, "PeriodMicroseconds"),
                    TraceLoggingBool(m_results.UsedMmcss ? TRUE : FALSE, "UsedMmcss"),
                    TraceLoggingBool(m_results.HighResolutionTimer ? TRUE : FALSE, "HighResolutionTimer"));
            }

            m_results.Ran = true;
            SetEvent(m_startedEvent);

            auto const frequency = Frequency();
            auto const period = (std::max)(MicrosecondsToTicks(m_periodMicroseconds, frequency), uint64_t{ 1 });

            HANDLE const handles[]{ m_stopEvent, timerHandle };

            auto deadline = Now() + period;

            for (;;)
            {
                auto const now = Now();

                // still late from the last wake: skip the periods that have already gone by
                if (now >= deadline)
                {
                    auto const behind = (now - deadline) / period + 1;

                    m_results.MissedWakes += behind;
                    deadline += behind * period;
                }

                // relative, in 100 ns units
                LARGE_INTEGER due{};
                due.QuadPart = -static_cast<LONGLONG>((std::max)(((deadline - now) * 10'000'000) / frequency, uint64_t{ 1 }));

                if (!SetWaitableTimer(timerHandle, &due, 0, nullptr, nullptr, FALSE))
                {
                    m_results.Error = GetLastError();
                    break;
                }

                auto const result = WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);

                if (result != WAIT_OBJECT_0 + 1)
                {
                    break;
                }

                auto const woke = Now();
                auto const late = woke > deadline ? woke - deadline : 0;
                auto const microseconds = TicksToMicroseconds(late, frequency);

                // A late wake goes in the trace too, so the check can see what the processor
                // that runs the timer was doing between the deadline and the wake.
                if (m_providerRegistered && microseconds >= ProbeLateWakeMicroseconds)
                {
                    TraceLoggingWrite(
                        g_latencyCheckProvider,
                        "ProbeLateWake",
                        TraceLoggingUInt64(deadline, "Deadline"),
                        TraceLoggingUInt64(woke, "Woke"));
                }

                m_results.Lateness.Add(late, microseconds, woke, GetCurrentProcessorNumber());

                if (m_lateness.size() < MaximumLatenessSamples)
                {
                    m_lateness.push_back(static_cast<uint32_t>((std::min)(microseconds, uint64_t{ UINT32_MAX })));
                }

                ++m_wakes;
                deadline += period;
            }

            CancelWaitableTimer(timerHandle);
        }
        catch (...)
        {
            // nothing may leave a thread
        }

        if (mmcss != nullptr)
        {
            AvRevertMmThreadCharacteristics(mmcss);
        }

        // in case it failed before it said it was running
        if (m_startedEvent != nullptr)
        {
            SetEvent(m_startedEvent);
        }
    }
}
