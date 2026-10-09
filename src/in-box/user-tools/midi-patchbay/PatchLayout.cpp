// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "PatchLayout.h"

#include <algorithm>
#include <limits>
#include <set>
#include <unordered_map>

namespace midipatchbay
{
    namespace
    {
        constexpr double LayoutMargin = 48.0;
        constexpr double ColumnGap = 80.0;
        constexpr double RowGap = 32.0;

        // Matches the canvas: a header, a column header, then a row per connection point.
        constexpr double EndpointWidth = 280.0;
        constexpr double EndpointFixedHeight = 68.0;
        constexpr double EndpointRowHeight = 32.0;

        // A header, a line, and two lines of what the step does.
        constexpr double BlockWidth = 208.0;
        constexpr double BlockHeight = 99.0;

        // Under that, a Branch or a Switch has a line and a row for each way out.
        constexpr double WayRowHeight = 24.0;
        constexpr double WaysSpacing = 10.0;

        // Roughly how wide a character of an annotation is, against its font size, and the space
        // around the text.
        constexpr double AnnotationCharacterWidth = 0.55;
        constexpr double AnnotationLineHeight = 1.35;
        constexpr double AnnotationPadding = 12.0;
    }

    _Use_decl_annotations_
    NodeSize EstimatedNodeSize(PatchDocument const& patch, std::wstring const& nodeId) noexcept
    {
        if (auto const* endpoint = patch.FindEndpoint(nodeId))
        {
            // All groups, plus group 1 or every group.
            auto const rows = endpoint->ShowAllGroups ? 1 + MaximumGroupCount : 2;

            return { EndpointWidth, EndpointFixedHeight + rows * EndpointRowHeight };
        }

        if (auto const* block = patch.FindBlock(nodeId); block != nullptr && IsAnnotation(block->Kind))
        {
            auto const& note = block->Settings.Annotation;

            // The longest line sets the width, and each line break adds a line.
            size_t longest{ 0 };
            size_t lines{ 1 };
            size_t current{ 0 };

            for (auto const ch : note.Text)
            {
                if (ch == L'\n')
                {
                    lines++;
                    current = 0;
                    continue;
                }

                longest = (std::max)(longest, ++current);
            }

            auto const characters = static_cast<double>((std::max)(longest, size_t{ 8 }));

            return { characters * note.FontSize * AnnotationCharacterWidth + AnnotationPadding,
                static_cast<double>(lines) * note.FontSize * AnnotationLineHeight + AnnotationPadding };
        }

        if (auto const* block = patch.FindBlock(nodeId); block != nullptr && HasWays(block->Kind))
        {
            try
            {
                auto const ways = static_cast<double>(WaysOf(block->Kind, block->Settings).size());

                return { BlockWidth, BlockHeight + WaysSpacing + ways * WayRowHeight };
            }
            catch (...)
            {
            }
        }

        return { BlockWidth, BlockHeight };
    }

    _Use_decl_annotations_
    void ArrangeInColumns(PatchDocument& patch, NodeSizeProvider const& sizeOf) noexcept
    {
        try
        {
            std::vector<std::wstring> ids{};

            for (auto const& endpoint : patch.Endpoints)
            {
                ids.push_back(endpoint.Id);
            }

            for (auto const& block : patch.Blocks)
            {
                // An annotation stays where the customer put it.
                if (IsAnnotation(block.Kind))
                {
                    continue;
                }

                ids.push_back(block.Id);
            }

            auto const count = ids.size();

            if (count == 0)
            {
                return;
            }

            std::unordered_map<std::wstring, size_t> indexOf{};

            for (size_t i = 0; i < count; i++)
            {
                indexOf.emplace(ids[i], i);
            }

            std::vector<std::vector<size_t>> next(count);

            for (auto const& connection : patch.Connections)
            {
                auto const source = indexOf.find(connection.SourceId);
                auto const destination = indexOf.find(connection.DestinationId);

                if (source == indexOf.end() || destination == indexOf.end() || source->second == destination->second)
                {
                    continue;
                }

                next[source->second].push_back(destination->second);
            }

            // A walk that finds the links closing a loop, so they can be left out of the order.
            std::set<std::pair<size_t, size_t>> loopLinks{};
            std::vector<uint8_t> state(count, 0);

            for (size_t root = 0; root < count; root++)
            {
                if (state[root] != 0)
                {
                    continue;
                }

                std::vector<std::pair<size_t, size_t>> stack{};
                stack.emplace_back(root, 0);
                state[root] = 1;

                while (!stack.empty())
                {
                    auto const node = stack.back().first;
                    auto const child = stack.back().second;

                    if (child < next[node].size())
                    {
                        stack.back().second++;

                        auto const target = next[node][child];

                        if (state[target] == 1)
                        {
                            loopLinks.emplace(node, target);
                        }
                        else if (state[target] == 0)
                        {
                            state[target] = 1;
                            stack.emplace_back(target, 0);
                        }
                    }
                    else
                    {
                        state[node] = 2;
                        stack.pop_back();
                    }
                }
            }

            auto const isForward = [&loopLinks](size_t from, size_t to)
            {
                return loopLinks.find({ from, to }) == loopLinks.end();
            };

            std::vector<std::vector<size_t>> previous(count);
            std::vector<size_t> waiting(count, 0);

            for (size_t from = 0; from < count; from++)
            {
                for (auto const to : next[from])
                {
                    if (isForward(from, to))
                    {
                        previous[to].push_back(from);
                        waiting[to]++;
                    }
                }
            }

            // The longest way in decides the column.
            std::vector<size_t> column(count, 0);
            std::vector<size_t> order{};

            for (size_t i = 0; i < count; i++)
            {
                if (waiting[i] == 0)
                {
                    order.push_back(i);
                }
            }

            for (size_t position = 0; position < order.size(); position++)
            {
                auto const from = order[position];

                for (auto const to : next[from])
                {
                    if (!isForward(from, to))
                    {
                        continue;
                    }

                    column[to] = (std::max)(column[to], column[from] + 1);

                    if (--waiting[to] == 0)
                    {
                        order.push_back(to);
                    }
                }
            }

            size_t lastColumn = 0;

            for (auto const value : column)
            {
                lastColumn = (std::max)(lastColumn, value);
            }

            // An endpoint that only receives goes to the far right, so every destination lines up.
            auto const endpointCount = patch.Endpoints.size();

            for (size_t i = 0; i < endpointCount; i++)
            {
                auto const sends = std::any_of(next[i].begin(), next[i].end(),
                    [&](size_t to) { return isForward(i, to); });

                if (!sends && !previous[i].empty())
                {
                    column[i] = lastColumn;
                }
            }

            std::vector<std::vector<size_t>> columns(lastColumn + 1);

            for (size_t i = 0; i < count; i++)
            {
                columns[column[i]].push_back(i);
            }

            std::vector<NodeSize> sizes(count);

            for (size_t i = 0; i < count; i++)
            {
                sizes[i] = sizeOf ? sizeOf(ids[i]) : EstimatedNodeSize(patch, ids[i]);
            }

            // The first column keeps the order the customer had, top to bottom.
            std::vector<double> currentY(count, 0.0);

            for (size_t i = 0; i < count; i++)
            {
                if (auto const* y = patch.NodeY(ids[i]))
                {
                    currentY[i] = *y;
                }
            }

            std::vector<double> left(count, 0.0);
            std::vector<double> top(count, 0.0);
            std::vector<double> middle(count, 0.0);
            std::vector<bool> placed(count, false);

            auto x = LayoutMargin;

            for (auto& members : columns)
            {
                if (members.empty())
                {
                    continue;
                }

                std::vector<double> wanted(count, (std::numeric_limits<double>::max)());

                for (auto const i : members)
                {
                    double total{ 0 };
                    size_t feeding{ 0 };

                    for (auto const from : previous[i])
                    {
                        if (placed[from])
                        {
                            total += middle[from];
                            feeding++;
                        }
                    }

                    if (feeding > 0)
                    {
                        wanted[i] = total / static_cast<double>(feeding);
                    }
                }

                std::stable_sort(members.begin(), members.end(), [&](size_t a, size_t b)
                {
                    if (wanted[a] != wanted[b])
                    {
                        return wanted[a] < wanted[b];
                    }

                    return currentY[a] < currentY[b];
                });

                double width{ 0 };
                auto y = LayoutMargin;

                for (auto const i : members)
                {
                    width = (std::max)(width, sizes[i].Width);

                    auto nodeTop = y;

                    if (wanted[i] != (std::numeric_limits<double>::max)())
                    {
                        nodeTop = (std::max)(y, wanted[i] - sizes[i].Height / 2.0);
                    }

                    left[i] = x;
                    top[i] = nodeTop;
                    middle[i] = nodeTop + sizes[i].Height / 2.0;
                    placed[i] = true;

                    y = nodeTop + sizes[i].Height + RowGap;
                }

                x += width + ColumnGap;
            }

            for (size_t i = 0; i < count; i++)
            {
                if (auto* nodeX = patch.NodeX(ids[i]))
                {
                    *nodeX = left[i];
                }

                if (auto* nodeY = patch.NodeY(ids[i]))
                {
                    *nodeY = top[i];
                }
            }
        }
        catch (...)
        {
            // Positions are only a convenience. A layout that cannot be built leaves them alone.
        }
    }
}
