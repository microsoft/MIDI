// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Where things go on the canvas when the app places them itself: after a version 1 patch is
// converted, and when the customer asks it to arrange the canvas. Pure, so the tests can check
// that nothing lands on top of anything else.

#include "PatchDocument.h"

#include <functional>

namespace midipatchbay
{
    struct NodeSize
    {
        double Width{ 0 };
        double Height{ 0 };
    };

    // The size a node has on the canvas. The canvas passes real sizes; a conversion, which runs
    // before there is a canvas, uses EstimatedNodeSize.
    using NodeSizeProvider = std::function<NodeSize(std::wstring const& nodeId)>;

    // Close enough for a first layout. The canvas arranges again with real sizes when asked.
    NodeSize EstimatedNodeSize(_In_ PatchDocument const& patch, _In_ std::wstring const& nodeId) noexcept;

    // Left to right in the order messages flow: what only sends first, each block after what
    // feeds it, and what only receives last. Each node sits as close as it can to the middle of
    // what feeds it. A loop still gets a layout: the link that closes it is left out of the
    // ordering. Only positions change.
    void ArrangeInColumns(_Inout_ PatchDocument& patch, _In_ NodeSizeProvider const& sizeOf) noexcept;
}
