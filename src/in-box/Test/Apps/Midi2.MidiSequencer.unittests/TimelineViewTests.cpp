// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TimelineViewTests.h"

#include "TimelineView.h"

#include <cmath>
#include <limits>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = 3840;

    // 26 bars of room, one bar to all of it across.
    TimelineLimits Limits()
    {
        return TimelineLimits{ 960.0, 26.0 * Bar, 26.0 * Bar };
    }

    bool Near(double a, double b, double within = 1.0e-6)
    {
        return std::abs(a - b) <= within;
    }
}

void TimelineViewTests::TheViewStaysInsideItsLimits()
{
    auto const limits = Limits();

    auto view = ClampView(TimelineSpan{ -5000.0, 4.0 * Bar }, limits);
    VERIFY_IS_TRUE(Near(view.Start, 0.0));

    view = ClampView(TimelineSpan{ 40.0 * Bar, 4.0 * Bar }, limits);
    VERIFY_IS_TRUE(Near(view.End(), limits.End));

    view = ClampView(TimelineSpan{ 0.0, 100.0 * Bar }, limits);
    VERIFY_IS_TRUE(Near(view.Span, limits.MaxSpan));

    view = ClampView(TimelineSpan{ 0.0, 10.0 }, limits);
    VERIFY_IS_TRUE(Near(view.Span, limits.MinSpan));
}

void TimelineViewTests::ZoomKeepsTheAnchorWhereItWas()
{
    auto const limits = Limits();
    TimelineSpan const view{ 4.0 * Bar, 12.0 * Bar };

    // The anchor at a third of the way across stays a third of the way across.
    auto const anchor = view.Start + view.Span / 3.0;
    auto const zoomed = ZoomAbout(view, 2.0, anchor, limits);

    VERIFY_IS_TRUE(Near(zoomed.Span, 6.0 * Bar));
    VERIFY_IS_TRUE(Near((anchor - zoomed.Start) / zoomed.Span, 1.0 / 3.0));

    // In and back out again comes back to where it was.
    auto const back = ZoomAbout(zoomed, 0.5, anchor, limits);
    VERIFY_IS_TRUE(Near(back.Start, view.Start, 1.0e-3));
    VERIFY_IS_TRUE(Near(back.Span, view.Span, 1.0e-3));
}

void TimelineViewTests::ZoomStopsAtItsLimits()
{
    auto const limits = Limits();
    TimelineSpan const view{ 4.0 * Bar, 12.0 * Bar };

    VERIFY_IS_TRUE(Near(ZoomAbout(view, 1000.0, 5.0 * Bar, limits).Span, limits.MinSpan));
    VERIFY_IS_TRUE(Near(ZoomAbout(view, 0.001, 5.0 * Bar, limits).Span, limits.MaxSpan));

    // All the way out is all of it, from the start.
    VERIFY_IS_TRUE(Near(ZoomAbout(view, 0.001, 5.0 * Bar, limits).Start, 0.0));
}

void TimelineViewTests::DraggingAnEndKeepsTheOtherEnd()
{
    auto const limits = Limits();
    TimelineSpan const view{ 4.0 * Bar, 12.0 * Bar };

    auto const right = DragViewEnd(view, true, 20.0 * Bar, limits);
    VERIFY_IS_TRUE(Near(right.Start, view.Start));
    VERIFY_IS_TRUE(Near(right.End(), 20.0 * Bar));

    auto const left = DragViewEnd(view, false, 2.0 * Bar, limits);
    VERIFY_IS_TRUE(Near(left.End(), view.End()));
    VERIFY_IS_TRUE(Near(left.Start, 2.0 * Bar));

    // An end dragged past the other stops at the narrowest view.
    auto const crossed = DragViewEnd(view, true, 0.0, limits);
    VERIFY_IS_TRUE(Near(crossed.Start, view.Start));
    VERIFY_IS_TRUE(Near(crossed.Span, limits.MinSpan));

    auto const crossedLeft = DragViewEnd(view, false, 30.0 * Bar, limits);
    VERIFY_IS_TRUE(Near(crossedLeft.End(), view.End()));
    VERIFY_IS_TRUE(Near(crossedLeft.Span, limits.MinSpan));

    // Past the start or the end of what there is, it stops there.
    VERIFY_IS_TRUE(Near(DragViewEnd(view, false, -10.0 * Bar, limits).Start, 0.0));
    VERIFY_IS_TRUE(Near(DragViewEnd(view, true, 90.0 * Bar, limits).End(), limits.End));
}

void TimelineViewTests::FittingShowsTheWholeRange()
{
    auto const limits = Limits();

    auto const all = FitView(0.0, 20.0 * Bar, Bar, limits);
    VERIFY_IS_TRUE(all.Start <= 0.0 + 1.0e-6);
    VERIFY_IS_TRUE(all.End() >= 20.0 * Bar);
    VERIFY_IS_TRUE(all.Span < 22.0 * Bar);

    auto const clip = FitView(8.0 * Bar, 12.0 * Bar, Bar, limits);
    VERIFY_IS_TRUE(clip.Start <= 8.0 * Bar);
    VERIFY_IS_TRUE(clip.End() >= 12.0 * Bar);
    VERIFY_IS_TRUE(clip.Span < 6.0 * Bar);

    // Nothing to fit shows eight bars from the start.
    auto const empty = FitView(0.0, 0.0, Bar, limits);
    VERIFY_IS_TRUE(Near(empty.Start, 0.0));
    VERIFY_IS_TRUE(Near(empty.Span, 8.0 * Bar));
}

void TimelineViewTests::TheExtentHasRoomAndTheView()
{
    VERIFY_IS_TRUE(Near(TimelineExtent(20 * Bar, Bar, TimelineSpan{ 0.0, 4.0 * Bar }), 24.0 * Bar));

    // An empty or short sequence still has sixteen bars to work in.
    VERIFY_IS_TRUE(Near(TimelineExtent(0, Bar, TimelineSpan{ 0.0, 0.0 }), 16.0 * Bar));
    VERIFY_IS_TRUE(Near(TimelineExtent(5 * Bar, Bar, TimelineSpan{ 0.0, 2.0 * Bar }), 16.0 * Bar));

    // Scrolled past the end, the view's end is the extent.
    VERIFY_IS_TRUE(Near(TimelineExtent(20 * Bar, Bar, TimelineSpan{ 30.0 * Bar, 4.0 * Bar }), 34.0 * Bar));
}

void TimelineViewTests::TheBarsScaleRoundTrips()
{
    TimelineBarScale const scale{ 100.0, 600.0, 30.0 * Bar };

    VERIFY_IS_TRUE(Near(scale.XAt(0.0), 100.0));
    VERIFY_IS_TRUE(Near(scale.XAt(30.0 * Bar), 700.0));
    VERIFY_IS_TRUE(Near(scale.TickAt(scale.XAt(12345.0)), 12345.0, 1.0e-6));
}

void TimelineViewTests::NonsenseLeavesASensibleView()
{
    auto const limits = Limits();
    auto const nan = std::numeric_limits<double>::quiet_NaN();
    TimelineSpan const view{ 4.0 * Bar, 12.0 * Bar };

    auto const clamped = ClampView(TimelineSpan{ nan, nan }, limits);
    VERIFY_IS_TRUE(std::isfinite(clamped.Start) && std::isfinite(clamped.Span));

    // A zoom by nothing, or by a broken factor, leaves the view as it was.
    VERIFY_IS_TRUE(Near(ZoomAbout(view, 0.0, 0.0, limits).Span, view.Span));
    VERIFY_IS_TRUE(Near(ZoomAbout(view, nan, 0.0, limits).Span, view.Span));
    VERIFY_IS_TRUE(Near(ZoomAbout(view, -2.0, 0.0, limits).Start, view.Start));
}
