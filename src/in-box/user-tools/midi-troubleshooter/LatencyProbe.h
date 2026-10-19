// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The DPC latency check's own test thread. It does what an audio app's audio thread does: it
// asks the Multimedia Class Scheduler Service (MMCSS) for "Pro Audio" priority, and it's woken by
// a high-resolution timer once every period, the way an audio thread is woken for every buffer.
// It does no other work. How long each wake took to start running shows how late audio could
// have been, even with no audio app open.
//
// It changes nothing on the PC: no timer resolution, no power settings.

#include "LatencyModel.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace miditroubleshooter::latency
{
    class Probe
    {
    public:
        Probe() = default;
        ~Probe();

        Probe(Probe const&) = delete;
        Probe& operator=(Probe const&) = delete;

        // Starts the thread and waits until it's running. Returns a Win32 error code.
        uint32_t Start(_In_ uint32_t const periodMicroseconds) noexcept;

        // Stops the thread and fills in the results. Safe to call more than once.
        void Stop() noexcept;

        // The thread's id once Start has succeeded, so the analyzer knows which one it is.
        uint32_t ThreadId() const noexcept { return m_threadId.load(); }

        // How many times it has woken so far. Safe from any thread.
        uint64_t Wakes() const noexcept { return m_wakes.load(); }

        // Valid after Stop.
        ProbeResults const& Results() const noexcept { return m_results; }

    private:
        void Run() noexcept;
        void FinishResults() noexcept;

        std::thread m_thread{};
        HANDLE m_stopEvent{ nullptr };
        HANDLE m_startedEvent{ nullptr };

        std::atomic<uint32_t> m_threadId{ 0 };
        std::atomic<uint64_t> m_wakes{ 0 };

        uint32_t m_periodMicroseconds{ 1000 };
        bool m_providerRegistered{ false };

        // written by the thread until it ends, then read by Stop
        ProbeResults m_results{};

        // how late each wake was, in microseconds, for the median and the 99th percentile
        std::vector<uint32_t> m_lateness{};
    };
}
