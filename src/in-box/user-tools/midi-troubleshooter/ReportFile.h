// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The parser lives with mididiag, so that partners can copy it from one place.
#include "..\mididiag\mididiag_report_parser.h"

namespace miditroubleshooter
{
    enum class ReportLoadError
    {
        None,
        CannotRead,
        Empty,
        TooLarge,
        NotAReport,
        ZipUnreadable,
        ZipHasNoReport
    };

    struct LoadedReport
    {
        ReportLoadError Error{ ReportLoadError::None };

        // Never changed once it's loaded, and shared, so the viewer can build each section only
        // when somebody opens it.
        std::shared_ptr<::mididiag::report::Report const> Report{};

        // The file it came from. Empty for a report this app ran.
        std::wstring FilePath{};

        // The file inside the zip that it came from. Empty when it wasn't in a zip.
        std::wstring EntryName{};
    };

    // A report this app has just run.
    LoadedReport LoadReportText(_In_ std::wstring_view const text) noexcept;

    // A report file, or a zip with one in it, such as a zip a customer attached to an issue.
    // Blocking, so it's called from a background thread. A zip is read with the tar program in
    // Windows, one text file at a time, and nothing from it is written to disk.
    LoadedReport LoadReportFile(_In_ std::wstring const& path) noexcept;

    // The Win32 open dialog, because the WinRT picker never completes in an elevated process.
    // Blocks until the dialog closes. Returns an empty string when the customer cancels.
    std::wstring ShowOpenReportDialog(_In_opt_ HWND const owner) noexcept;

    // What to tell the customer when a report couldn't be loaded.
    winrt::hstring ReportLoadErrorMessage(_In_ ReportLoadError const error) noexcept;
}
