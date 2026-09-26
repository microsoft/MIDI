// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "ArrangeOps.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace glass
{
    namespace
    {
        // Positions along the axis being arranged, sorted, as indexes into the caller's list.
        // Everything here works in that order and writes back in the caller's order, so a
        // selection made by clicking in a random order still spaces correctly.
        std::vector<size_t> OrderAlong(_In_ std::vector<EditRect> const& rects, _In_ ArrangeAxis axis)
        {
            std::vector<size_t> order(rects.size());
            std::iota(order.begin(), order.end(), size_t{ 0 });

            std::stable_sort(
                order.begin(),
                order.end(),
                [&rects, axis](size_t left, size_t right)
                {
                    return axis == ArrangeAxis::Horizontal
                        ? rects[left].X < rects[right].X
                        : rects[left].Y < rects[right].Y;
                });

            return order;
        }
    }

    _Use_decl_annotations_
    std::vector<EditRect> AlignRects(std::vector<EditRect> const& rects, AlignEdge edge)
    {
        auto result = rects;

        if (result.size() < 2)
        {
            return result;
        }

        switch (edge)
        {
        case AlignEdge::Left:
        {
            auto const target = std::min_element(
                rects.begin(), rects.end(), [](auto const& a, auto const& b) { return a.X < b.X; })->X;

            for (auto& rect : result) { rect.X = target; }
            break;
        }

        case AlignEdge::Right:
        {
            auto const target = std::max_element(
                rects.begin(), rects.end(), [](auto const& a, auto const& b) { return a.Right() < b.Right(); })->Right();

            for (auto& rect : result) { rect.X = target - rect.Width; }
            break;
        }

        case AlignEdge::CenterX:
        {
            double total{ 0.0 };
            for (auto const& rect : rects) { total += rect.CenterX(); }

            auto const target = total / static_cast<double>(rects.size());

            for (auto& rect : result) { rect.X = target - rect.Width / 2.0; }
            break;
        }

        case AlignEdge::Top:
        {
            auto const target = std::min_element(
                rects.begin(), rects.end(), [](auto const& a, auto const& b) { return a.Y < b.Y; })->Y;

            for (auto& rect : result) { rect.Y = target; }
            break;
        }

        case AlignEdge::Bottom:
        {
            auto const target = std::max_element(
                rects.begin(), rects.end(), [](auto const& a, auto const& b) { return a.Bottom() < b.Bottom(); })->Bottom();

            for (auto& rect : result) { rect.Y = target - rect.Height; }
            break;
        }

        case AlignEdge::CenterY:
        {
            double total{ 0.0 };
            for (auto const& rect : rects) { total += rect.CenterY(); }

            auto const target = total / static_cast<double>(rects.size());

            for (auto& rect : result) { rect.Y = target - rect.Height / 2.0; }
            break;
        }
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<double> MeasureGaps(std::vector<EditRect> const& rects, ArrangeAxis axis)
    {
        std::vector<double> gaps{};

        if (rects.size() < 2)
        {
            return gaps;
        }

        auto const order = OrderAlong(rects, axis);

        gaps.reserve(order.size() - 1);

        for (size_t index = 1; index < order.size(); ++index)
        {
            auto const& previous = rects[order[index - 1]];
            auto const& current = rects[order[index]];

            gaps.push_back(axis == ArrangeAxis::Horizontal
                ? current.X - previous.Right()
                : current.Y - previous.Bottom());
        }

        return gaps;
    }

    _Use_decl_annotations_
    bool GapsAreEqual(std::vector<double> const& gaps) noexcept
    {
        if (gaps.size() < 2)
        {
            return true;
        }

        for (size_t index = 1; index < gaps.size(); ++index)
        {
            if (std::abs(gaps[index] - gaps[0]) > 1.0)
            {
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    std::vector<EditRect> SetGap(std::vector<EditRect> const& rects, ArrangeAxis axis, double gap)
    {
        auto result = rects;

        if (result.size() < 2 || !std::isfinite(gap))
        {
            return result;
        }

        auto const order = OrderAlong(rects, axis);

        auto cursor = axis == ArrangeAxis::Horizontal
            ? rects[order[0]].Right()
            : rects[order[0]].Bottom();

        for (size_t index = 1; index < order.size(); ++index)
        {
            auto& rect = result[order[index]];

            if (axis == ArrangeAxis::Horizontal)
            {
                rect.X = cursor + gap;
                cursor = rect.Right();
            }
            else
            {
                rect.Y = cursor + gap;
                cursor = rect.Bottom();
            }
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<EditRect> DistributeEvenly(std::vector<EditRect> const& rects, ArrangeAxis axis)
    {
        if (rects.size() < 3)
        {
            return rects;
        }

        auto const order = OrderAlong(rects, axis);

        auto const& first = rects[order.front()];
        auto const& last = rects[order.back()];

        double span{ 0.0 };
        double occupied{ 0.0 };

        if (axis == ArrangeAxis::Horizontal)
        {
            span = last.Right() - first.X;
            for (auto const index : order) { occupied += rects[index].Width; }
        }
        else
        {
            span = last.Bottom() - first.Y;
            for (auto const index : order) { occupied += rects[index].Height; }
        }

        auto const gap = (span - occupied) / static_cast<double>(order.size() - 1);

        return SetGap(rects, axis, gap);
    }

    _Use_decl_annotations_
    std::vector<size_t> ReorderForZ(
        size_t count,
        std::vector<size_t> const& selected,
        ZOrderMove move)
    {
        std::vector<size_t> order(count);
        std::iota(order.begin(), order.end(), size_t{ 0 });

        std::vector<bool> isSelected(count, false);

        for (auto const index : selected)
        {
            if (index < count)
            {
                isSelected[index] = true;
            }
        }

        if (count < 2)
        {
            return order;
        }

        switch (move)
        {
        case ZOrderMove::ToFront:
        case ZOrderMove::ToBack:
        {
            std::vector<size_t> moved{};
            std::vector<size_t> stayed{};

            for (auto const index : order)
            {
                (isSelected[index] ? moved : stayed).push_back(index);
            }

            order.clear();

            if (move == ZOrderMove::ToBack)
            {
                order.insert(order.end(), moved.begin(), moved.end());
                order.insert(order.end(), stayed.begin(), stayed.end());
            }
            else
            {
                order.insert(order.end(), stayed.begin(), stayed.end());
                order.insert(order.end(), moved.begin(), moved.end());
            }

            break;
        }

        case ZOrderMove::Forward:
        {
            // From the top down, so a run of selected controls slides as one and the topmost
            // one of them stops the rest rather than being jumped over.
            for (size_t i = count - 1; i-- > 0;)
            {
                if (isSelected[order[i]] && !isSelected[order[i + 1]])
                {
                    std::swap(order[i], order[i + 1]);
                }
            }

            break;
        }

        case ZOrderMove::Backward:
        {
            for (size_t i = 1; i < count; ++i)
            {
                if (isSelected[order[i]] && !isSelected[order[i - 1]])
                {
                    std::swap(order[i], order[i - 1]);
                }
            }

            break;
        }
        }

        return order;
    }
}
