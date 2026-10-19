// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Runs a DPC latency check from start to finish: recording on this PC now, or reading a trace
// saved earlier. The page starts it, shows how it's going, and shows what it found. Everything
// here blocks, so the page calls it from a background thread.

#include "LatencyAnalyzer.h"
#include "LatencyAssessment.h"
#include "LatencyModel.h"
#include "LatencyProbe.h"
#include "LatencyTraceSession.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace miditroubleshooter::latency
{
    struct CheckOptions
    {
        // Run the test thread, which stands in for an audio app's audio thread.
        bool UseProbe{ true };
        uint32_t ProbePeriodMicroseconds{ 1000 };

        // Profile samples, to see whose code ran inside Windows' shared drivers.
        bool CollectSamples{ true };

        // Also save everything to this .etl file, for Windows Performance Analyzer. Empty for none.
        std::wstring TraceFilePath{};
        uint32_t MaximumFileMegabytes{ 2048 };
    };

    struct CheckOutcome
    {
        // A Win32 error when the check couldn't record or couldn't read the trace.
        uint32_t Error{ 0 };

        bool Canceled{ false };

        Results Data{};
        Assessment Summary{};

        // the session that recorded a live check
        std::optional<SessionInfo> Session{};

        bool Succeeded() const noexcept { return Error == 0 && !Canceled; }
    };

    // A check recording on this PC. Needs administrator rights.
    class LiveCheck
    {
    public:
        LiveCheck() = default;
        ~LiveCheck();

        LiveCheck(LiveCheck const&) = delete;
        LiveCheck& operator=(LiveCheck const&) = delete;

        // Starts the trace session, the test thread and the reader. Returns a Win32 error code.
        // When it fails, nothing is left running.
        uint32_t Start(_In_ CheckOptions const& options) noexcept;

        // How it's going. Safe from any thread.
        LiveStatus Status() const;
        uint64_t ProbeWakes() const noexcept { return m_probe.Wakes(); }

        // True when recording stopped by itself, such as when another program stopped the session.
        bool RecordingEnded() const noexcept { return m_readerDone.load(); }

        // Stops recording and works out what it found: a few seconds.
        CheckOutcome Finish() noexcept;

        // Stops everything and throws the results away. Safe to call more than once, and from
        // any thread. While Start is running, this waits for it, and once this has been called,
        // Start won't start anything.
        void Abandon() noexcept;

        // Stops only the trace session, which would otherwise keep recording until Windows
        // restarts. Safe from any thread at any time, even while Finish runs, so a window that's
        // closing can call it.
        void StopSession() noexcept { m_session.Stop(); }

    private:
        void AbandonLocked() noexcept;
        void StopReader(_In_ bool const deliverEverything) noexcept;

        // Start, Finish and Abandon take turns.
        std::mutex m_lifecycleLock{};
        std::atomic<bool> m_abandoned{ false };

        CheckOptions m_options{};
        KernelSession m_session{};
        TraceReader m_reader{};
        Probe m_probe{};

        // Shared, because the page asks for the status from its own thread while Finish runs.
        mutable std::mutex m_analyzerLock{};
        std::shared_ptr<Analyzer> m_analyzer{};

        std::thread m_readerThread{};
        std::atomic<bool> m_readerDone{ false };
        uint32_t m_readerStatus{ 0 };

        bool m_started{ false };
    };

    // Reads a trace saved earlier, by this check or by Windows Performance Recorder. Doesn't need
    // administrator rights. Set cancel from another thread to stop it. eventsRead counts as it goes.
    CheckOutcome AnalyzeTraceFile(
        _In_ std::wstring const& path,
        _In_ std::atomic<bool> const& cancel,
        _Inout_ std::atomic<uint64_t>& eventsRead) noexcept;
}
