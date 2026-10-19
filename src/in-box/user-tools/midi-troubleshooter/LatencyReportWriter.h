// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Writes the results of a DPC latency check as a report in mididiag's layout (report format 2),
// so the Troubleshooter's report viewer, and any tool that reads mididiag reports, can show it,
// and a person can read it as plain text. The rules for the layout are at the top of
// mididiag_field_defs.h.
//
// Like mididiag, the labels, keys and values are for programs and stay in English. Only the
// text of each finding is in the customer's language, and the app passes that in.

#include "LatencyAssessment.h"
#include "LatencyModel.h"
#include "LatencyTraceSession.h"

#include <optional>
#include <string>
#include <vector>

namespace miditroubleshooter::latency
{
    // The first line of every report, so a reader can tell it from a mididiag report.
    inline constexpr wchar_t ReportProductName[]{ L"Microsoft Windows MIDI Services - DPC Latency Check Report" };

    // in the header section, so tools can tell the two kinds of report apart
    inline constexpr wchar_t ReportType[]{ L"dpc_latency_check" };

    struct ReportFinding
    {
        // stable across versions and languages, such as "long_driver"
        std::wstring Id{};

        // a sentence for a person, in the customer's language
        std::wstring Text{};
    };

    // What the report says that the results don't know.
    struct ReportContext
    {
        std::wstring ToolVersion{};

        // local time, as YYYY-MM-DD HH:MM:SS
        std::wstring CurrentTime{};

        bool RunningElevated{ false };

        // about this PC, for a check run on it
        std::wstring OsVersion{};
        std::wstring Architecture{};
        std::wstring ProcessorName{};

        // the session that recorded a live check
        std::optional<SessionInfo> Session{};

        // where the live check saved its trace, when it saved one
        std::wstring SavedTracePath{};

        std::vector<ReportFinding> Findings{};
    };

    std::wstring WriteReport(
        _In_ Results const& results,
        _In_ Assessment const& assessment,
        _In_ ReportContext const& context);

    // Stable names, used in the report and by the app.
    std::wstring_view VerdictName(_In_ Verdict const verdict) noexcept;
    std::wstring_view MomentKindName(_In_ MomentKind const kind) noexcept;
    std::wstring_view ThreadKindName(_In_ AudioThreadKind const kind) noexcept;
}
