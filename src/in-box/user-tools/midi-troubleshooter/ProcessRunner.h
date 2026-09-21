// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    struct ProcessResult
    {
        bool Started{ false };
        bool TimedOut{ false };

        // set by StartAndWatch when the program was still going at the end of the settle time
        bool StillRunning{ false };

        DWORD ExitCode{ 0 };

        // stdout and stderr, interleaved in the order the child wrote them
        std::wstring Output{};

        // set when the process could not be started at all
        std::wstring ErrorMessage{};
    };

    // Runs a console program with its output redirected to a pipe and no console window.
    // Blocking: callers run it from a background thread.
    ProcessResult RunCapture(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::chrono::seconds timeout) noexcept;

    // Runs a console program without capturing anything, for the tools that write their own
    // output files. Also blocking.
    ProcessResult RunToCompletion(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::chrono::seconds timeout) noexcept;

    // Starts a program which is meant to keep running until something else stops it - a
    // recorder - and waits only long enough to see whether it failed straight away. The
    // program is left running either way. Nothing is captured, because closing the read end
    // of a pipe underneath a child which is still writing would break it.
    ProcessResult StartAndWatch(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::chrono::seconds settleTime) noexcept;
}
