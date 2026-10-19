// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What part of the sequence the timeline shows, in ticks, and the arithmetic the timeline's scroll
// bar does on it (design section 31): zooming about a point that stays still, dragging either end
// of the view, and fitting. No WinRT here, so the tests take it as it is.

#include <cstdint>

namespace midisequencer
{
    struct TimelineSpan
    {
        double Start{ 0 };   // the tick at the timeline's left edge
        double Span{ 1 };    // how many ticks across

        double End() const noexcept { return Start + Span; }
    };

    struct TimelineLimits
    {
        double MinSpan{ 1 };   // zoomed all the way in
        double MaxSpan{ 1 };   // zoomed all the way out
        double End{ 1 };       // the view can't start so late that it ends past this
    };

    // How many ticks the sequence's scroll bar covers: the sequence and four bars after it, at
    // least sixteen bars, and the view, wherever it is.
    double TimelineExtent(_In_ int64_t sequenceEnd, _In_ int64_t ticksPerBar, _In_ TimelineSpan const& view) noexcept;

    // Within the limits: the span first, then the start.
    TimelineSpan ClampView(_In_ TimelineSpan view, _In_ TimelineLimits const& limits) noexcept;

    // Zoomed in by the factor (above 1 zooms in), with the tick at the anchor where it was.
    TimelineSpan ZoomAbout(_In_ TimelineSpan const& view, _In_ double factor, _In_ double anchorTick, _In_ TimelineLimits const& limits) noexcept;

    // One end dragged to a tick, the other staying where it was.
    TimelineSpan DragViewEnd(_In_ TimelineSpan const& view, _In_ bool rightEnd, _In_ double tick, _In_ TimelineLimits const& limits) noexcept;

    // The whole of what's from the first tick to the last, with a little room after.
    TimelineSpan FitView(_In_ double firstTick, _In_ double lastTick, _In_ int64_t ticksPerBar, _In_ TimelineLimits const& limits) noexcept;

    // The scroll bar's track: ticks across a strip of pixels.
    struct TimelineBarScale
    {
        double Left{ 0 };
        double Width{ 1 };
        double Extent{ 1 };

        double XAt(_In_ double tick) const noexcept { return Left + tick / Extent * Width; }
        double TickAt(_In_ double x) const noexcept { return (x - Left) / Width * Extent; }
    };
}
