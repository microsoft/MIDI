// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <sal.h>

#include "TimelineView.h"

#include <algorithm>
#include <cmath>

namespace midisequencer
{
    namespace
    {
        // Room after the sequence, to drag a clip past its end, and room enough to start one.
        constexpr double RoomBars = 4.0;
        constexpr double LeastBars = 16.0;
    }

    _Use_decl_annotations_
    double TimelineExtent(int64_t sequenceEnd, int64_t ticksPerBar, TimelineSpan const& view) noexcept
    {
        auto const bar = static_cast<double>(std::max<int64_t>(1, ticksPerBar));
        auto const content = std::max(static_cast<double>(std::max<int64_t>(0, sequenceEnd)) + bar * RoomBars, bar * LeastBars);
        return std::max(content, view.End());
    }

    _Use_decl_annotations_
    TimelineSpan ClampView(TimelineSpan view, TimelineLimits const& limits) noexcept
    {
        auto const most = std::max(limits.MinSpan, limits.MaxSpan);

        if (!std::isfinite(view.Span))
        {
            view.Span = most;
        }

        if (!std::isfinite(view.Start))
        {
            view.Start = 0;
        }

        view.Span = std::clamp(view.Span, limits.MinSpan, most);
        view.Start = std::clamp(view.Start, 0.0, std::max(0.0, limits.End - view.Span));
        return view;
    }

    _Use_decl_annotations_
    TimelineSpan ZoomAbout(TimelineSpan const& view, double factor, double anchorTick, TimelineLimits const& limits) noexcept
    {
        if (!(factor > 0) || !std::isfinite(factor) || !(view.Span > 0))
        {
            return ClampView(view, limits);
        }

        auto const most = std::max(limits.MinSpan, limits.MaxSpan);
        auto const span = std::clamp(view.Span / factor, limits.MinSpan, most);
        auto const share = (anchorTick - view.Start) / view.Span;

        return ClampView(TimelineSpan{ anchorTick - share * span, span }, limits);
    }

    _Use_decl_annotations_
    TimelineSpan DragViewEnd(TimelineSpan const& view, bool rightEnd, double tick, TimelineLimits const& limits) noexcept
    {
        auto const most = std::max(limits.MinSpan, limits.MaxSpan);

        if (rightEnd)
        {
            auto const end = std::min(tick, limits.End);
            auto const span = std::clamp(end - view.Start, limits.MinSpan, most);
            return ClampView(TimelineSpan{ view.Start, span }, limits);
        }

        auto const end = view.End();
        auto const start = std::max(0.0, tick);
        auto const span = std::clamp(end - start, limits.MinSpan, most);
        return ClampView(TimelineSpan{ end - span, span }, limits);
    }

    _Use_decl_annotations_
    TimelineSpan FitView(double firstTick, double lastTick, int64_t ticksPerBar, TimelineLimits const& limits) noexcept
    {
        auto const bar = static_cast<double>(std::max<int64_t>(1, ticksPerBar));
        auto const first = std::max(0.0, std::min(firstTick, lastTick));
        auto const last = std::max(firstTick, lastTick);

        // An empty sequence fits eight bars.
        auto const span = last > first ? (last - first) * 1.04 + bar * 0.5 : bar * 8.0;
        auto const room = (span - (last - first)) / 2.0;

        return ClampView(TimelineSpan{ last > first ? first - room : 0.0, span }, limits);
    }
}
