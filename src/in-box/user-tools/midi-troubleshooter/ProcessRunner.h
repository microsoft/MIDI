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
        DWORD ExitCode{ 0 };

        // stdout and stderr, interleaved in the order the child wrote them
        std::wstring Output{};

        // set when the process could not be started at all
        std::wstring ErrorMessage{};
    };

    // Runs a console program with its output redirected to a pipe and no console window.
    // Blocking: callers run it from a background thread.
    ProcessResult RunCapture(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout) noexcept;

    // Runs a console program without capturing anything, for the tools that write their own
    // output files. Also blocking.
    ProcessResult RunToCompletion(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout) noexcept;
}
