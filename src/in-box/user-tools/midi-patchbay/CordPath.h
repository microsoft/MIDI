// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Where a connection's line runs on the canvas: the usual curve from an Out to an In when nothing
// is in its way, and around the endpoints and steps that are, rather than through them. Pure, so
// the tests can check that a line never crosses a node.

#include <sal.h>

#include <cstddef>
#include <vector>

namespace midipatchbay
{
    struct CordPoint
    {
        double X{ 0 };
        double Y{ 0 };
    };

    struct CordRect
    {
        double Left{ 0 };
        double Top{ 0 };
        double Right{ 0 };
        double Bottom{ 0 };
    };

    // One piece of a line, a cubic curve from where the piece before it ended. A straight piece
    // has its control points on it.
    struct CordPiece
    {
        CordPoint Control1{};
        CordPoint Control2{};
        CordPoint End{};
        bool IsStraight{ false };
    };

    struct CordPath
    {
        CordPoint Start{};
        std::vector<CordPiece> Pieces{};

        // On the line, where its group label goes.
        CordPoint LabelPoint{};

        // It goes around something instead of taking the usual curve.
        bool GoesAround{ false };
    };

    // For an end of a line that isn't on a node, such as the loose end of one being drawn.
    constexpr size_t NoNode = static_cast<size_t>(-1);

    // From an Out, which a line leaves to the right, to an In, which a line comes into from the
    // left. `nodes` is every endpoint and step on the canvas. The line keeps out of all of them,
    // except where it leaves `sourceNode` and where it arrives at `destinationNode`.
    CordPath PlanCordPath(
        _In_ CordPoint const& from,
        _In_ CordPoint const& to,
        _In_ std::vector<CordRect> const& nodes,
        _In_ size_t sourceNode,
        _In_ size_t destinationNode) noexcept;

    struct CordRequest
    {
        CordPoint From{};
        CordPoint To{};
        size_t SourceNode{ NoNode };
        size_t DestinationNode{ NoNode };

        // Whether the line shows a label, so the others can keep their labels clear of it.
        bool HasLabel{ true };
    };

    // Every line on the canvas at once, in the same order. Lines that go around things can find
    // the same way past them, so where they would be drawn over one another they're spread
    // apart. Empty if it couldn't finish.
    std::vector<CordPath> PlanCordPaths(
        _In_ std::vector<CordRequest> const& requests,
        _In_ std::vector<CordRect> const& nodes) noexcept;

    // Points along the line, close enough together to find what's near it. The first is its start.
    std::vector<CordPoint> SampleCordPath(_In_ CordPath const& path, _In_ int samplesPerCurve) noexcept;

    // The curve a line takes when nothing is in its way, the one earlier versions always drew.
    CordPath UsualCordPath(_In_ CordPoint const& from, _In_ CordPoint const& to) noexcept;
}
