// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The kernel trace session the DPC latency check records with, and the reader that feeds its
// events to the analyzer, live or from a saved file.
//
// The session is an ordinary Event Tracing for Windows session in "system logger" mode, which
// gets kernel events without taking the one NT Kernel Logger session other tools use. Starting
// it needs administrator rights. Reading a saved file doesn't.

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace miditroubleshooter::latency
{
    struct SessionOptions
    {
        bool CollectSamples{ true };
        bool CollectHardFaults{ true };

        // Also writes everything to this .etl file, for Windows Performance Analyzer. Empty for none.
        std::wstring TraceFilePath{};

        // The file stops growing at this size. Recording continues; only the file stops.
        uint32_t MaximumFileMegabytes{ 1024 };
    };

    // What the session ended up recording, for the report.
    struct SessionInfo
    {
        bool Started{ false };
        uint32_t StartError{ 0 };

        bool SamplesEnabled{ false };
        uint32_t ProfilePrivilegeError{ 0 };

        bool FrameworkEventsEnabled{ false };
        uint32_t FrameworkEventsError{ 0 };

        bool ProbeProviderEnabled{ false };

        // the profile timer interval Windows was already using, in 100 ns units
        uint32_t ProfileInterval{ 0 };

        // the kernel's own record of which groups were on, for anyone checking a strange result
        std::wstring GroupMask{};

        // from the session when it stopped
        uint64_t EventsLost{ 0 };
        uint64_t BuffersLost{ 0 };
        uint64_t RealTimeBuffersLost{ 0 };

        bool WritingFile{ false };
    };

    class KernelSession
    {
    public:
        static constexpr wchar_t SessionName[]{ L"Windows MIDI Services DPC Latency Check" };

        KernelSession() = default;
        ~KernelSession();

        KernelSession(KernelSession const&) = delete;
        KernelSession& operator=(KernelSession const&) = delete;

        // Starts the session. Returns a Win32 error code. A session left running by an earlier
        // copy of this app that didn't close cleanly is stopped first.
        uint32_t Start(_In_ SessionOptions const& options) noexcept;

        // Stops recording. Whatever is still in the buffers is delivered to the reader, and then
        // its Process call returns. Safe to call more than once, and from any thread.
        void Stop() noexcept;

        // Stops a session with this check's name, if one is running. Needs administrator rights.
        static void StopLeftoverSession() noexcept;

        SessionInfo const& Info() const noexcept { return m_info; }

    private:
        std::atomic<uint64_t> m_handle{ 0 };
        SessionInfo m_info{};
        bool m_restoreProfilePrivilege{ false };
    };

    struct TraceHeader
    {
        uint64_t Frequency{ 0 };
        uint32_t ProcessorCount{ 0 };
        uint32_t PointerSize{ 8 };
        uint32_t ClockType{ 1 };
        uint32_t WindowsBuild{ 0 };
        uint64_t EventsLost{ 0 };
        uint64_t BuffersLost{ 0 };
    };

    class TraceReader
    {
    public:
        using Callback = std::function<void(EVENT_RECORD const&)>;

        TraceReader() = default;
        ~TraceReader();

        TraceReader(TraceReader const&) = delete;
        TraceReader& operator=(TraceReader const&) = delete;

        // Returns a Win32 error code.
        uint32_t OpenFile(_In_ std::wstring const& path) noexcept;
        uint32_t OpenLive(_In_ std::wstring const& sessionName) noexcept;

        TraceHeader const& Header() const noexcept { return m_header; }

        // Delivers every event, on the calling thread, until the file ends or the live session
        // stops. Returns a Win32 error code.
        uint32_t Process(_In_ Callback callback) noexcept;

        // Ends Process early, from another thread.
        void Cancel() noexcept;

    private:
        static void WINAPI OnEventRecord(_In_ PEVENT_RECORD record);

        uint32_t Open(_Inout_ EVENT_TRACE_LOGFILEW& logFile) noexcept;

        std::atomic<uint64_t> m_handle{ INVALID_PROCESSTRACE_HANDLE };
        TraceHeader m_header{};
        Callback m_callback{};
    };
}
