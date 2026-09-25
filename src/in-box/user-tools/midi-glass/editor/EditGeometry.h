// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. Where a control lands when somebody drags it is
// arithmetic, and it is the arithmetic a customer notices first.

#include <sal.h>
#include <cstdint>
#include <vector>

namespace glass
{
    struct EditRect
    {
        double X{ 0.0 };
        double Y{ 0.0 };
        double Width{ 0.0 };
        double Height{ 0.0 };

        double Right() const noexcept { return X + Width; }
        double Bottom() const noexcept { return Y + Height; }
        double CenterX() const noexcept { return X + Width / 2.0; }
        double CenterY() const noexcept { return Y + Height / 2.0; }
    };

    bool Intersects(_In_ EditRect const& a, _In_ EditRect const& b) noexcept;
    bool Contains(_In_ EditRect const& outer, _In_ EditRect const& inner) noexcept;
    bool ContainsPoint(_In_ EditRect const& rect, _In_ double x, _In_ double y) noexcept;

    // Any part hanging over an edge counts, the same rule the document uses. A control half off
    // the page is just as unreachable at run time as one entirely off it.
    bool IsOutsidePage(_In_ EditRect const& rect, _In_ double pageWidth, _In_ double pageHeight) noexcept;

    double SnapToGrid(_In_ double value, _In_ double gridSize) noexcept;

    // The smallest a control is allowed to get by dragging. Typing a smaller number in the
    // inspector is the customer's business; a handle that can shrink a control to nothing is not.
    constexpr double MinimumControlSize = 8.0;
    enum class ResizeHandle
    {
        None = 0,
        TopLeft = 1,
        Top = 2,
        TopRight = 3,
        Right = 4,
        BottomRight = 5,
        Bottom = 6,
        BottomLeft = 7,
        Left = 8,
    };

    bool MovesLeftEdge(_In_ ResizeHandle handle) noexcept;
    bool MovesRightEdge(_In_ ResizeHandle handle) noexcept;
    bool MovesTopEdge(_In_ ResizeHandle handle) noexcept;
    bool MovesBottomEdge(_In_ ResizeHandle handle) noexcept;

    // Corner handles can hold the aspect ratio; an edge handle cannot, because only one
    // dimension is being dragged and there is nothing to hold it against.
    bool IsCornerHandle(_In_ ResizeHandle handle) noexcept;

    EditRect ApplyResize(
        _In_ EditRect const& start,
        _In_ ResizeHandle handle,
        _In_ double deltaX,
        _In_ double deltaY,
        _In_ bool preserveAspect) noexcept;

    enum class GuideAxis
    {
        // A vertical line, matching left edges, centers or right edges.
        Vertical = 0,

        Horizontal = 1,
    };

    // One pink line the canvas draws while a drag is snapped to something. Start and End are the
    // extent to draw along the other axis, so the line reaches both the thing being dragged and
    // the thing it lined up with.
    struct SnapGuide
    {
        GuideAxis Axis{ GuideAxis::Vertical };
        double Position{ 0.0 };
        double Start{ 0.0 };
        double End{ 0.0 };
    };

    struct SnapSettings
    {
        bool GridEnabled{ true };
        double GridSize{ 8.0 };

        // Edges, centers and the page margins. Off with the grid when Alt is held.
        bool GuidesEnabled{ true };

        // How near, in page pixels, before something pulls.
        double Threshold{ 6.0 };
    };

    struct SnapOutcome
    {
        double X{ 0.0 };
        double Y{ 0.0 };
        std::vector<SnapGuide> Guides{};
    };

    // Where a rectangle being moved should actually land.
    //
    // A guide beats the grid. Somebody lining a control up with the one beside it means that
    // edge, not the nearest multiple of eight, and a drawing app that did it the other way round
    // would feel broken.
    SnapOutcome SnapMove(
        _In_ EditRect const& moving,
        _In_ std::vector<EditRect> const& others,
        _In_ EditRect const& page,
        _In_ SnapSettings const& settings) noexcept;

    // The same, for the edge or corner a handle is dragging. Only the edges that actually move
    // are snapped, so dragging the right edge never shifts the left one.
    SnapOutcome SnapResize(
        _In_ EditRect const& moving,
        _In_ ResizeHandle handle,
        _In_ std::vector<EditRect> const& others,
        _In_ EditRect const& page,
        _In_ SnapSettings const& settings) noexcept;

    // Where existing controls sit when the page changes size. Nine positions, read in rows.
    enum class CanvasAnchor
    {
        TopLeft = 0,
        Top = 1,
        TopRight = 2,
        Left = 3,
        Center = 4,
        Right = 5,
        BottomLeft = 6,
        Bottom = 7,
        BottomRight = 8,
    };

    // What to do to every control when the page size changes: x' = x * Scale + OffsetX.
    struct PageResizeTransform
    {
        double Scale{ 1.0 };
        double OffsetX{ 0.0 };
        double OffsetY{ 0.0 };
    };

    // Growing asks where the existing controls should sit. Shrinking offers to scale them, and
    // "leave them" is the same transform with an anchor of TopLeft and no scaling — which is why
    // there is one function rather than a grow path and a shrink path.
    PageResizeTransform ComputePageResize(
        _In_ double oldWidth,
        _In_ double oldHeight,
        _In_ double newWidth,
        _In_ double newHeight,
        _In_ CanvasAnchor anchor,
        _In_ bool scaleContents) noexcept;

    EditRect ApplyTransform(_In_ EditRect const& rect, _In_ PageResizeTransform const& transform) noexcept;

    // How many of these would end up outside the new page. The number the shrink dialog quotes,
    // worked out before anything is committed.
    size_t CountOutsidePage(
        _In_ std::vector<EditRect> const& rects,
        _In_ PageResizeTransform const& transform,
        _In_ double pageWidth,
        _In_ double pageHeight) noexcept;

    // The work area the editor draws around the page. Big enough that a control parked outside
    // is visible and reachable, rather than being somewhere off in an infinite scroll.
    constexpr double CanvasMarginFraction = 0.12;
    constexpr double MinimumCanvasMargin = 64.0;

    EditRect ComputeWorkArea(
        _In_ double pageWidth,
        _In_ double pageHeight,
        _In_ std::vector<EditRect> const& rects) noexcept;
}
