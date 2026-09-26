// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "EditGeometry.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        // One place a drag can land, and what to draw if it does.
        struct Candidate
        {
            // Where the moving edge would end up.
            double Position{ 0.0 };

            // The extent of the thing it lined up with, so the guide reaches both.
            double Start{ 0.0 };
            double End{ 0.0 };
        };

        struct BestSnap
        {
            bool Found{ false };
            double Delta{ 0.0 };
            double Position{ 0.0 };
            double Start{ 0.0 };
            double End{ 0.0 };
        };

        void Consider(
            _Inout_ BestSnap& best,
            _In_ double movingEdge,
            _In_ double candidate,
            _In_ double start,
            _In_ double end,
            _In_ double threshold) noexcept
        {
            auto const delta = candidate - movingEdge;

            if (std::abs(delta) > threshold)
            {
                return;
            }

            if (best.Found && std::abs(best.Delta) <= std::abs(delta))
            {
                return;
            }

            best.Found = true;
            best.Delta = delta;
            best.Position = candidate;
            best.Start = start;
            best.End = end;
        }

        // Vertical lines a rectangle offers: its left edge, its center and its right edge.
        void CollectVertical(_In_ EditRect const& rect, _Inout_ std::vector<Candidate>& into)
        {
            into.push_back({ rect.X, rect.Y, rect.Bottom() });
            into.push_back({ rect.CenterX(), rect.Y, rect.Bottom() });
            into.push_back({ rect.Right(), rect.Y, rect.Bottom() });
        }

        void CollectHorizontal(_In_ EditRect const& rect, _Inout_ std::vector<Candidate>& into)
        {
            into.push_back({ rect.Y, rect.X, rect.Right() });
            into.push_back({ rect.CenterY(), rect.X, rect.Right() });
            into.push_back({ rect.Bottom(), rect.X, rect.Right() });
        }

        double SpanStart(_In_ double a, _In_ double b) noexcept { return std::min(a, b); }
        double SpanEnd(_In_ double a, _In_ double b) noexcept { return std::max(a, b); }
    }

    _Use_decl_annotations_
    bool Intersects(EditRect const& a, EditRect const& b) noexcept
    {
        return a.X < b.Right() && b.X < a.Right() && a.Y < b.Bottom() && b.Y < a.Bottom();
    }

    _Use_decl_annotations_
    bool Contains(EditRect const& outer, EditRect const& inner) noexcept
    {
        return inner.X >= outer.X &&
            inner.Y >= outer.Y &&
            inner.Right() <= outer.Right() &&
            inner.Bottom() <= outer.Bottom();
    }

    _Use_decl_annotations_
    bool ContainsPoint(EditRect const& rect, double x, double y) noexcept
    {
        return x >= rect.X && x <= rect.Right() && y >= rect.Y && y <= rect.Bottom();
    }

    _Use_decl_annotations_
    bool IsOutsidePage(EditRect const& rect, double pageWidth, double pageHeight) noexcept
    {
        return rect.X < 0.0 ||
            rect.Y < 0.0 ||
            rect.Right() > pageWidth ||
            rect.Bottom() > pageHeight;
    }

    _Use_decl_annotations_
    double SnapToGrid(double value, double gridSize) noexcept
    {
        if (!std::isfinite(value) || !std::isfinite(gridSize) || gridSize <= 0.0)
        {
            return value;
        }

        return std::round(value / gridSize) * gridSize;
    }

    _Use_decl_annotations_
    bool MovesLeftEdge(ResizeHandle handle) noexcept
    {
        return handle == ResizeHandle::TopLeft || handle == ResizeHandle::Left || handle == ResizeHandle::BottomLeft;
    }

    _Use_decl_annotations_
    bool MovesRightEdge(ResizeHandle handle) noexcept
    {
        return handle == ResizeHandle::TopRight || handle == ResizeHandle::Right || handle == ResizeHandle::BottomRight;
    }

    _Use_decl_annotations_
    bool MovesTopEdge(ResizeHandle handle) noexcept
    {
        return handle == ResizeHandle::TopLeft || handle == ResizeHandle::Top || handle == ResizeHandle::TopRight;
    }

    _Use_decl_annotations_
    bool MovesBottomEdge(ResizeHandle handle) noexcept
    {
        return handle == ResizeHandle::BottomLeft || handle == ResizeHandle::Bottom || handle == ResizeHandle::BottomRight;
    }

    _Use_decl_annotations_
    bool IsCornerHandle(ResizeHandle handle) noexcept
    {
        return handle == ResizeHandle::TopLeft ||
            handle == ResizeHandle::TopRight ||
            handle == ResizeHandle::BottomLeft ||
            handle == ResizeHandle::BottomRight;
    }

    _Use_decl_annotations_
    EditRect ApplyResize(
        EditRect const& start,
        ResizeHandle handle,
        double deltaX,
        double deltaY,
        bool preserveAspect) noexcept
    {
        if (handle == ResizeHandle::None)
        {
            return start;
        }

        auto left = start.X;
        auto top = start.Y;
        auto right = start.Right();
        auto bottom = start.Bottom();

        if (MovesLeftEdge(handle))
        {
            left = std::min(start.X + deltaX, right - MinimumControlSize);
        }

        if (MovesRightEdge(handle))
        {
            right = std::max(start.Right() + deltaX, left + MinimumControlSize);
        }

        if (MovesTopEdge(handle))
        {
            top = std::min(start.Y + deltaY, bottom - MinimumControlSize);
        }

        if (MovesBottomEdge(handle))
        {
            bottom = std::max(start.Bottom() + deltaY, top + MinimumControlSize);
        }

        EditRect result{ left, top, right - left, bottom - top };

        if (preserveAspect && IsCornerHandle(handle) && start.Width > 0.0 && start.Height > 0.0)
        {
            auto const ratio = start.Width / start.Height;

            // Whichever axis was dragged furthest wins, so the corner follows the pointer
            // rather than fighting it.
            if (std::abs(result.Width - start.Width) >= std::abs(result.Height - start.Height))
            {
                result.Height = std::max(MinimumControlSize, result.Width / ratio);
            }
            else
            {
                result.Width = std::max(MinimumControlSize, result.Height * ratio);
            }

            if (MovesLeftEdge(handle))
            {
                result.X = right - result.Width;
            }

            if (MovesTopEdge(handle))
            {
                result.Y = bottom - result.Height;
            }
        }

        return result;
    }

    _Use_decl_annotations_
    SnapOutcome SnapMove(
        EditRect const& moving,
        std::vector<EditRect> const& others,
        EditRect const& page,
        SnapSettings const& settings) noexcept
    {
        SnapOutcome outcome{ moving.X, moving.Y, {} };

        BestSnap bestVertical{};
        BestSnap bestHorizontal{};

        if (settings.GuidesEnabled)
        {
            std::vector<Candidate> vertical{};
            std::vector<Candidate> horizontal{};

            vertical.reserve((others.size() + 1) * 3);
            horizontal.reserve((others.size() + 1) * 3);

            CollectVertical(page, vertical);
            CollectHorizontal(page, horizontal);

            for (auto const& other : others)
            {
                CollectVertical(other, vertical);
                CollectHorizontal(other, horizontal);
            }

            double const movingVertical[]{ moving.X, moving.CenterX(), moving.Right() };
            double const movingHorizontal[]{ moving.Y, moving.CenterY(), moving.Bottom() };

            for (auto const& candidate : vertical)
            {
                for (auto const edge : movingVertical)
                {
                    Consider(
                        bestVertical,
                        edge,
                        candidate.Position,
                        SpanStart(candidate.Start, moving.Y),
                        SpanEnd(candidate.End, moving.Bottom()),
                        settings.Threshold);
                }
            }

            for (auto const& candidate : horizontal)
            {
                for (auto const edge : movingHorizontal)
                {
                    Consider(
                        bestHorizontal,
                        edge,
                        candidate.Position,
                        SpanStart(candidate.Start, moving.X),
                        SpanEnd(candidate.End, moving.Right()),
                        settings.Threshold);
                }
            }
        }

        if (bestVertical.Found)
        {
            outcome.X = moving.X + bestVertical.Delta;
            outcome.Guides.push_back({ GuideAxis::Vertical, bestVertical.Position, bestVertical.Start, bestVertical.End });
        }
        else if (settings.GridEnabled)
        {
            outcome.X = SnapToGrid(moving.X, settings.GridSize);
        }

        if (bestHorizontal.Found)
        {
            outcome.Y = moving.Y + bestHorizontal.Delta;
            outcome.Guides.push_back({ GuideAxis::Horizontal, bestHorizontal.Position, bestHorizontal.Start, bestHorizontal.End });
        }
        else if (settings.GridEnabled)
        {
            outcome.Y = SnapToGrid(moving.Y, settings.GridSize);
        }

        return outcome;
    }

    _Use_decl_annotations_
    SnapOutcome SnapResize(
        EditRect const& moving,
        ResizeHandle handle,
        std::vector<EditRect> const& others,
        EditRect const& page,
        SnapSettings const& settings) noexcept
    {
        // The caller gets back the adjusted position of the edges that moved. X carries the
        // vertical edge and Y the horizontal one, whichever of the two ends the handle drags.
        SnapOutcome outcome{};

        outcome.X = MovesLeftEdge(handle) ? moving.X : moving.Right();
        outcome.Y = MovesTopEdge(handle) ? moving.Y : moving.Bottom();

        BestSnap bestVertical{};
        BestSnap bestHorizontal{};

        if (settings.GuidesEnabled)
        {
            std::vector<Candidate> vertical{};
            std::vector<Candidate> horizontal{};

            CollectVertical(page, vertical);
            CollectHorizontal(page, horizontal);

            for (auto const& other : others)
            {
                CollectVertical(other, vertical);
                CollectHorizontal(other, horizontal);
            }

            if (MovesLeftEdge(handle) || MovesRightEdge(handle))
            {
                for (auto const& candidate : vertical)
                {
                    Consider(
                        bestVertical,
                        outcome.X,
                        candidate.Position,
                        SpanStart(candidate.Start, moving.Y),
                        SpanEnd(candidate.End, moving.Bottom()),
                        settings.Threshold);
                }
            }

            if (MovesTopEdge(handle) || MovesBottomEdge(handle))
            {
                for (auto const& candidate : horizontal)
                {
                    Consider(
                        bestHorizontal,
                        outcome.Y,
                        candidate.Position,
                        SpanStart(candidate.Start, moving.X),
                        SpanEnd(candidate.End, moving.Right()),
                        settings.Threshold);
                }
            }
        }

        if (bestVertical.Found)
        {
            outcome.X = bestVertical.Position;
            outcome.Guides.push_back({ GuideAxis::Vertical, bestVertical.Position, bestVertical.Start, bestVertical.End });
        }
        else if (settings.GridEnabled && (MovesLeftEdge(handle) || MovesRightEdge(handle)))
        {
            outcome.X = SnapToGrid(outcome.X, settings.GridSize);
        }

        if (bestHorizontal.Found)
        {
            outcome.Y = bestHorizontal.Position;
            outcome.Guides.push_back({ GuideAxis::Horizontal, bestHorizontal.Position, bestHorizontal.Start, bestHorizontal.End });
        }
        else if (settings.GridEnabled && (MovesTopEdge(handle) || MovesBottomEdge(handle)))
        {
            outcome.Y = SnapToGrid(outcome.Y, settings.GridSize);
        }

        return outcome;
    }

    _Use_decl_annotations_
    PageResizeTransform ComputePageResize(
        double oldWidth,
        double oldHeight,
        double newWidth,
        double newHeight,
        CanvasAnchor anchor,
        bool scaleContents) noexcept
    {
        PageResizeTransform transform{};

        if (oldWidth <= 0.0 || oldHeight <= 0.0 || newWidth <= 0.0 || newHeight <= 0.0)
        {
            return transform;
        }

        if (scaleContents)
        {
            // Both axes by the same factor. Scaling them independently would stretch a knob into
            // an ellipse, and a page is letterboxed at run time for exactly the same reason.
            transform.Scale = std::min(newWidth / oldWidth, newHeight / oldHeight);

            auto const scaledWidth = oldWidth * transform.Scale;
            auto const scaledHeight = oldHeight * transform.Scale;

            transform.OffsetX = (newWidth - scaledWidth) / 2.0;
            transform.OffsetY = (newHeight - scaledHeight) / 2.0;

            return transform;
        }

        auto const spareX = newWidth - oldWidth;
        auto const spareY = newHeight - oldHeight;

        auto const column = static_cast<int32_t>(anchor) % 3;
        auto const row = static_cast<int32_t>(anchor) / 3;

        transform.OffsetX = spareX * (column / 2.0);
        transform.OffsetY = spareY * (row / 2.0);

        return transform;
    }

    _Use_decl_annotations_
    EditRect ApplyTransform(EditRect const& rect, PageResizeTransform const& transform) noexcept
    {
        return
        {
            rect.X * transform.Scale + transform.OffsetX,
            rect.Y * transform.Scale + transform.OffsetY,
            rect.Width * transform.Scale,
            rect.Height * transform.Scale,
        };
    }

    _Use_decl_annotations_
    size_t CountOutsidePage(
        std::vector<EditRect> const& rects,
        PageResizeTransform const& transform,
        double pageWidth,
        double pageHeight) noexcept
    {
        size_t count{ 0 };

        for (auto const& rect : rects)
        {
            if (IsOutsidePage(ApplyTransform(rect, transform), pageWidth, pageHeight))
            {
                ++count;
            }
        }

        return count;
    }

    _Use_decl_annotations_
    EditRect ComputeWorkArea(
        double pageWidth,
        double pageHeight,
        std::vector<EditRect> const& rects) noexcept
    {
        auto const marginX = std::max(MinimumCanvasMargin, pageWidth * CanvasMarginFraction);
        auto const marginY = std::max(MinimumCanvasMargin, pageHeight * CanvasMarginFraction);

        auto left = -marginX;
        auto top = -marginY;
        auto right = pageWidth + marginX;
        auto bottom = pageHeight + marginY;

        // A control parked outside the page has to stay reachable, so the work area grows to
        // hold it. Clamping it inside instead would make a page resize unrecoverable.
        for (auto const& rect : rects)
        {
            left = std::min(left, rect.X - marginX);
            top = std::min(top, rect.Y - marginY);
            right = std::max(right, rect.Right() + marginX);
            bottom = std::max(bottom, rect.Bottom() + marginY);
        }

        return { left, top, right - left, bottom - top };
    }
}
