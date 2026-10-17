// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The words the DPC latency check page uses: what the results mean, which drivers held audio up
// and what to try. All of it comes from the app's resources, so it can be translated. The
// technical report's labels and values stay in English, like mididiag's.

#include "LatencyCheck.h"
#include "LatencyReportWriter.h"

#include <string>
#include <vector>

namespace miditroubleshooter::latency::text
{
    // "3.48 ms", in the customer's number format
    std::wstring Milliseconds(_In_ uint64_t const microseconds);

    // "1,234,567", in the customer's number format
    std::wstring Count(_In_ uint64_t const value);

    // A driver as a person would name it, such as "NVIDIA Windows Kernel Mode Driver
    // (nvlddmkm.sys)", or what kind of driver it is when its file can't be looked at.
    std::wstring DriverTitle(_In_ Results const& results, _In_ int32_t const module);

    // who made it, its version and the device it drives, when this PC knows
    std::wstring DriverDetails(_In_ Results const& results, _In_ int32_t const module);

    // how long it held a processor, and how often audio was waiting for it
    std::wstring CulpritImpact(_In_ Culprit const& culprit);

    // what to try about it
    std::wstring CulpritAdvice(_In_ Results const& results, _In_ Culprit const& culprit);

    // a driver that ran long but held nothing up, on one line
    std::wstring RanLongLine(_In_ Results const& results, _In_ Culprit const& culprit);

    std::wstring VerdictTitle(_In_ Assessment const& assessment);
    std::wstring VerdictMessage(_In_ Assessment const& assessment);

    // Empty when there's nothing to say, such as when nothing stood in for audio.
    std::wstring HeldUpLine(_In_ Assessment const& assessment);
    std::wstring MidiHeldUpLine(_In_ Assessment const& assessment);
    std::wstring BufferLine(_In_ Assessment const& assessment);

    // advice that isn't about one driver
    std::vector<std::wstring> ExtraAdvice(_In_ Assessment const& assessment);

    // what limited the check, and where its trace is
    std::vector<std::wstring> Notes(_In_ CheckOutcome const& outcome);

    // the findings section of the technical report
    std::vector<ReportFinding> Findings(_In_ CheckOutcome const& outcome);

    // UTF-8 with a byte order mark, like the app's other saved reports
    bool WriteUtf8File(_In_ std::wstring const& path, _In_ std::wstring const& text) noexcept;
}
