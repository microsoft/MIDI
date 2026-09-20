// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EndpointCatalog.h"
#include "PatchModel.h"

namespace midipatchbay
{
    enum class LoopSeverity
    {
        // Every endpoint on the circle is a loopback, so it is certain that what goes in comes
        // back out. These connections are held muted.
        Certain = 0,

        // The circle closes only if a device echoes what it receives, which this app cannot see.
        // Reported, not muted.
        Possible = 1,
    };

    struct LoopFinding
    {
        LoopSeverity Severity{ LoopSeverity::Possible };

        // In travel order, so the message can name the hops the way the customer sees them.
        std::vector<std::wstring> EndpointIds{};
        std::vector<std::wstring> EndpointNames{};

        // The connections that make up the circle.
        std::vector<std::wstring> ConnectionIds{};

        // Endpoints on the circle that are only assumed to echo. Empty for a certain loop.
        std::vector<std::wstring> AssumedEchoNames{};
    };

    struct PatchAnalysis
    {
        std::vector<LoopFinding> Loops{};

        // Connections held muted because they close a certain loop.
        std::unordered_set<std::wstring> LoopMutedConnectionIds{};

        bool HasCertainLoop() const noexcept
        {
            return std::any_of(Loops.begin(), Loops.end(),
                [](LoopFinding const& f) { return f.Severity == LoopSeverity::Certain; });
        }
    };

    // Walks the patch as a directed graph over (endpoint, group, side) and reports the circles.
    //
    // This only sees what Patchbay itself routes. A DIN cable between two devices, or another
    // router, can close a circle that is invisible here, which is why the UI says "no loops
    // detected" rather than "no loops".
    PatchAnalysis AnalyzePatch(
        _In_ PatchDocument const& patch,
        _In_ std::vector<LiveEndpoint> const& liveEndpoints) noexcept;

    // The same walk with one more connection added, for answering "can I drop this here" before
    // the connection is committed.
    bool WouldCreateCertainLoop(
        _In_ PatchDocument const& patch,
        _In_ PatchConnection const& proposed,
        _In_ std::vector<LiveEndpoint> const& liveEndpoints) noexcept;
}
