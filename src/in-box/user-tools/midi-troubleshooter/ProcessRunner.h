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

        // set by RunCaptureBytes when the program wrote more than it was allowed to, and was stopped
        bool OutputLimitReached{ false };

        DWORD ExitCode{ 0 };

        // stdout and stderr, interleaved in the order the child wrote them
        std::wstring Output{};

        // RunCaptureBytes only: exactly the bytes the child wrote to stdout
        std::string RawOutput{};

        // set when the process could not be started at all
        std::wstring ErrorMessage{};
    };

    // Called on the thread running the program with each piece of output as it arrives.
    // A piece never splits a character, but it often splits a line.
    using OutputReceivedHandler = std::function<void(std::wstring_view const text)>;

    // Runs a console program with its output redirected to a pipe and no console window.
    // Blocking: callers run it from a background thread.
    ProcessResult RunCapture(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::chrono::seconds timeout) noexcept;

    // The same, also handing output to the handler as it arrives. The result still carries all of it.
    ProcessResult RunCapture(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::chrono::seconds timeout,
        _In_ OutputReceivedHandler const& onOutputReceived) noexcept;

    // The same, started in the given folder. The tar in Windows reads its command line in the
    // system's ANSI code page, so it can't open a path with a character outside it, such as a
    // Japanese folder name on a PC set up for English. Run it in the file's folder and pass
    // only the file name, and the folder can have any name.
    ProcessResult RunCaptureIn(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::wstring const& workingFolder,
        _In_ std::chrono::seconds timeout) noexcept;

    // For a program that writes the contents of a file to stdout, such as tar extracting one
    // file from a zip. Keeps exactly the bytes it wrote, in RawOutput, and throws standard error
    // away so it can't mix in. Stops the program and sets OutputLimitReached as soon as it has
    // written more than maximumBytes, so a file that is far too large can't fill the memory.
    ProcessResult RunCaptureBytes(
        _In_ std::wstring const& executablePath,
        _In_ std::wstring const& arguments,
        _In_ std::wstring const& workingFolder,
        _In_ std::chrono::seconds timeout,
        _In_ size_t maximumBytes) noexcept;

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
