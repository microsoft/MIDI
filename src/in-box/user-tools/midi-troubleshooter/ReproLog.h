// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    struct CaptureOptions
    {
        // ddodiag, dxdiag and the exported plug and play state
        bool IncludeSystemInformation{ true };

        bool IncludeMidiDiag{ true };
        bool IncludeMidiKsInfo{ true };

        // Only offered when tttracer.exe is present. Shadow stack mitigations are not touched,
        // so an attach can still be refused; that is recorded in the package rather than
        // failing the capture.
        bool IncludeTimeTravelTrace{ false };
    };

    struct CaptureStepResult
    {
        bool Succeeded{ false };

        // one line per action taken, shown live in the app and worth keeping
        std::vector<std::wstring> Log{};

        std::wstring ErrorMessage{};
    };

    // Drives a repro capture: start tracing, let the customer reproduce the problem, then stop
    // and collect everything into one zip file. This is the in-app replacement for the
    // CollectMidiLogs script; providers.wprp is the only external file it still needs.
    // Every method blocks and is called from a background thread.
    class ReproCapture
    {
    public:
        bool IsRunning() const noexcept { return m_running; }

        std::wstring WorkingFolder() const noexcept { return m_workingFolder; }

        CaptureStepResult Start(CaptureOptions const& options) noexcept;

        // Stops tracing, gathers the diagnostic reports and writes the zip.
        CaptureStepResult Finish(std::wstring const& outputZipPath) noexcept;

        // Stops tracing and throws the working folder away.
        CaptureStepResult Cancel() noexcept;

        // A name a support engineer can read at a glance.
        static std::wstring SuggestedFileName() noexcept;

    private:
        void StopTracing(std::vector<std::wstring>& log) noexcept;
        void RemoveWorkingFolder() noexcept;

        bool m_running{ false };
        bool m_timeTravelTracingStarted{ false };
        std::wstring m_workingFolder{};
        CaptureOptions m_options{};
    };
}
