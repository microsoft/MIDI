// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "CordPath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <utility>

namespace midipatchbay
{
    namespace
    {
        // How close a line that goes around a node may come to it. Small enough that a line still
        // fits between two steps stacked the usual distance apart.
        constexpr double Clearance = 12.0;

        // How far it stays from what it goes around when there's room for more.
        constexpr double Comfort = 32.0;

        // How far a line runs straight out of an Out, and into an In, at the least.
        constexpr double Lead = 16.0;

        // The shortest run between two turns that tidying a line leaves.
        constexpr double MinimumRun = 8.0;

        // A turn costs as much as this much more line, so a line takes a few long runs instead of
        // a staircase of short ones.
        constexpr double TurnCost = 48.0;

        // The most a turn is rounded off, and how close the rounding may come to a node.
        constexpr double TurnRadius = 72.0;
        constexpr double CurveClearance = 6.0;

        // How far inside its own node a line may start or end, for a point measured a little in
        // from the node's edge.
        constexpr double EndTolerance = 2.0;

        // The usual curve, the same one earlier versions drew: how far its ends reach out.
        constexpr double ReachFactor = 0.55;
        constexpr double MinimumReach = 42.0;
        constexpr double MaximumReach = 140.0;

        // An In at least this far right of its Out tries the usual curve first.
        constexpr double ForwardDistance = 20.0;

        // The loop earlier versions drew back to an earlier node, for when no way around is found.
        constexpr double LoopDepth = 130.0;
        constexpr double LoopReach = 90.0;
        constexpr double LoopMiddleReach = 80.0;

        // Control points this far along make a cubic curve a quarter of an ellipse.
        constexpr double QuarterEllipse = 0.5522847498;

        // A label is wider than it is tall, so a level run suits it better. Roughly how much room
        // one takes, and where along a run to try putting it, so labels don't cover each other.
        constexpr double LevelLabelPreference = 1.5;
        constexpr double LabelWidth = 64.0;
        constexpr double LabelHeight = 22.0;
        constexpr double LabelFractions[] = { 0.5, 0.3, 0.7, 0.15, 0.85 };

        // How far apart runs of different lines that share a way past something are drawn.
        constexpr double RunSpacing = 10.0;
        constexpr size_t MaximumSpreadRuns = 20000;

        // Nothing left of the canvas or above it can be scrolled to, so a line that goes around
        // something keeps out of there, the same as it keeps out of a node.
        constexpr double Far = 1.0e7;
        constexpr CordRect OffCanvas[] = { CordRect{ -Far, -Far, 0, Far }, CordRect{ -Far, -Far, Far, 0 } };

        // How much work one line may take. Past this it gets the usual curve.
        constexpr size_t MaximumWalls = 160;
        constexpr size_t MaximumGridPoints = 160000;
        constexpr int MaximumRounds = 6;

        constexpr int TidyPasses = 3;
        constexpr int CheckSamples = 24;
        constexpr int TurnCheckSamples = 12;
        constexpr int TurnShrinkSteps = 8;
        constexpr double TurnShrink = 0.7;

        constexpr double Infinity = std::numeric_limits<double>::infinity();

        // Which way a run goes. A value and its reverse differ only in the lowest bit.
        enum class Heading : uint32_t
        {
            Right = 0,
            Left = 1,
            Down = 2,
            Up = 3,
        };

        constexpr uint32_t HeadingCount = 4;

        Heading Reverse(_In_ Heading heading) noexcept
        {
            return static_cast<Heading>(static_cast<uint32_t>(heading) ^ 1u);
        }

        bool IsFinite(_In_ CordPoint const& point) noexcept
        {
            return std::isfinite(point.X) && std::isfinite(point.Y);
        }

        bool IsFinite(_In_ CordRect const& rect) noexcept
        {
            return std::isfinite(rect.Left) && std::isfinite(rect.Top) &&
                std::isfinite(rect.Right) && std::isfinite(rect.Bottom) &&
                rect.Right >= rect.Left && rect.Bottom >= rect.Top;
        }

        CordRect Grown(_In_ CordRect const& rect, _In_ double by) noexcept
        {
            return CordRect{ rect.Left - by, rect.Top - by, rect.Right + by, rect.Bottom + by };
        }

        bool StrictlyInside(_In_ CordPoint const& point, _In_ CordRect const& rect) noexcept
        {
            return point.X > rect.Left && point.X < rect.Right && point.Y > rect.Top && point.Y < rect.Bottom;
        }

        bool Overlaps(_In_ CordRect const& a, _In_ CordRect const& b) noexcept
        {
            return a.Left < b.Right && b.Left < a.Right && a.Top < b.Bottom && b.Top < a.Bottom;
        }

        double Distance(_In_ CordPoint const& a, _In_ CordPoint const& b) noexcept
        {
            return std::hypot(b.X - a.X, b.Y - a.Y);
        }

        // One unit long, pointing from one point to the other.
        CordPoint Direction(_In_ CordPoint const& from, _In_ CordPoint const& to) noexcept
        {
            auto const length = Distance(from, to);

            return length > 0
                ? CordPoint{ (to.X - from.X) / length, (to.Y - from.Y) / length }
                : CordPoint{};
        }

        CordPoint PointOnPiece(_In_ CordPoint const& start, _In_ CordPiece const& piece, _In_ double t) noexcept
        {
            auto const u = 1.0 - t;
            auto const a = u * u * u;
            auto const b = 3 * u * u * t;
            auto const c = 3 * u * t * t;
            auto const d = t * t * t;

            return CordPoint{
                a * start.X + b * piece.Control1.X + c * piece.Control2.X + d * piece.End.X,
                a * start.Y + b * piece.Control1.Y + c * piece.Control2.Y + d * piece.End.Y };
        }

        CordPiece Straight(_In_ CordPoint const& from, _In_ CordPoint const& to) noexcept
        {
            return CordPiece{
                CordPoint{ from.X + (to.X - from.X) / 3, from.Y + (to.Y - from.Y) / 3 },
                CordPoint{ from.X + (to.X - from.X) * 2 / 3, from.Y + (to.Y - from.Y) * 2 / 3 },
                to,
                true };
        }

        void AppendStraight(_Inout_ CordPath& path, _In_ CordPoint const& from, _In_ CordPoint const& to)
        {
            if (Distance(from, to) > 0.01)
            {
                path.Pieces.push_back(Straight(from, to));
            }
        }

        // Whether the segment passes through the inside of the rectangle. Running along an edge or
        // touching a corner doesn't count.
        bool CrossesInside(_In_ CordPoint const& a, _In_ CordPoint const& b, _In_ CordRect const& rect) noexcept
        {
            auto low = 0.0;
            auto high = 1.0;

            auto const dx = b.X - a.X;
            auto const dy = b.Y - a.Y;

            // Each side cuts off part of the segment: p is how fast the segment moves out past
            // that side, and q how far inside it the segment starts.
            for (auto const& [p, q] : { std::pair{ -dx, a.X - rect.Left },
                                        std::pair{ dx, rect.Right - a.X },
                                        std::pair{ -dy, a.Y - rect.Top },
                                        std::pair{ dy, rect.Bottom - a.Y } })
            {
                if (p == 0)
                {
                    if (q < 0)
                    {
                        return false;
                    }

                    continue;
                }

                auto const t = q / p;

                if (p < 0)
                {
                    low = (std::max)(low, t);
                }
                else
                {
                    high = (std::min)(high, t);
                }

                if (low > high)
                {
                    return false;
                }
            }

            // The middle of the part inside. It's on the edge when the segment only runs along it.
            auto const t = (low + high) / 2;

            return StrictlyInside(CordPoint{ a.X + dx * t, a.Y + dy * t }, rect);
        }

        // The curve earlier versions always drew, and still the one used when nothing is in its way.
        CordPath UsualCurve(_In_ CordPoint const& from, _In_ CordPoint const& to)
        {
            auto const reach = std::clamp(std::abs(to.X - from.X) * ReachFactor, MinimumReach, MaximumReach);

            CordPath path{};
            path.Start = from;
            path.Pieces.push_back(CordPiece{
                CordPoint{ from.X + reach, from.Y },
                CordPoint{ to.X - reach, to.Y },
                to,
                false });
            path.LabelPoint = CordPoint{ (from.X + to.X) / 2, (from.Y + to.Y) / 2 };

            return path;
        }

        // What earlier versions drew back to an earlier node: a loop under both ends.
        CordPath LoopUnder(_In_ CordPoint const& from, _In_ CordPoint const& to)
        {
            auto const bottom = (std::max)(from.Y, to.Y) + LoopDepth;
            auto const middle = (from.X + to.X) / 2;

            CordPath path{};
            path.Start = from;
            path.Pieces.push_back(CordPiece{
                CordPoint{ from.X + LoopReach, from.Y },
                CordPoint{ middle + LoopMiddleReach, bottom },
                CordPoint{ middle, bottom },
                false });
            path.Pieces.push_back(CordPiece{
                CordPoint{ middle - LoopMiddleReach, bottom },
                CordPoint{ to.X - LoopReach, to.Y },
                to,
                false });
            path.LabelPoint = CordPoint{ middle, bottom };

            return path;
        }

        // Whether a line keeps out of every node, apart from where it leaves its source and
        // arrives at its destination.
        bool IsClear(
            _In_ CordPath const& path,
            _In_ std::vector<CordRect> const& nodes,
            _In_ size_t source,
            _In_ size_t destination)
        {
            auto const samples = SampleCordPath(path, CheckSamples);

            if (samples.size() < 2)
            {
                return false;
            }

            CordRect box{ Infinity, Infinity, -Infinity, -Infinity };

            for (auto const& sample : samples)
            {
                box.Left = (std::min)(box.Left, sample.X);
                box.Top = (std::min)(box.Top, sample.Y);
                box.Right = (std::max)(box.Right, sample.X);
                box.Bottom = (std::max)(box.Bottom, sample.Y);
            }

            for (size_t node = 0; node < nodes.size(); node++)
            {
                if (!IsFinite(nodes[node]))
                {
                    continue;
                }

                auto const area = node == source || node == destination
                    ? Grown(nodes[node], -EndTolerance)
                    : Grown(nodes[node], CurveClearance);

                if (!Overlaps(area, box))
                {
                    continue;
                }

                for (size_t i = 1; i < samples.size(); i++)
                {
                    if (CrossesInside(samples[i - 1], samples[i], area))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        // The space a line keeps out of around a node. When an end of the line falls in it,
        // because two nodes are close together, that side is pulled in to the end. A node with an
        // end inside it can't be kept out of, and has none.
        std::optional<CordRect> WallAround(
            _In_ CordRect const& node,
            _In_ CordPoint const& start,
            _In_ CordPoint const& goal) noexcept
        {
            auto wall = Grown(node, Clearance);

            for (auto const& end : { start, goal })
            {
                if (!StrictlyInside(end, wall))
                {
                    continue;
                }

                if (StrictlyInside(end, node))
                {
                    return std::nullopt;
                }

                auto const left = end.X - wall.Left;
                auto const right = wall.Right - end.X;
                auto const top = end.Y - wall.Top;
                auto const bottom = wall.Bottom - end.Y;
                auto const nearest = (std::min)({ left, right, top, bottom });

                if (nearest == left)
                {
                    wall.Left = end.X;
                }
                else if (nearest == right)
                {
                    wall.Right = end.X;
                }
                else if (nearest == top)
                {
                    wall.Top = end.Y;
                }
                else
                {
                    wall.Bottom = end.Y;
                }
            }

            return wall;
        }

        // The lines a run can follow: through each end, and along every side of every wall. Where
        // two cross is a place a line can turn, unless it's inside a wall.
        struct Grid
        {
            std::vector<double> Xs{};
            std::vector<double> Ys{};

            std::vector<uint8_t> Blocked{};
            std::vector<uint8_t> RightBlocked{};
            std::vector<uint8_t> DownBlocked{};

            size_t Index(_In_ size_t column, _In_ size_t row) const noexcept
            {
                return row * Xs.size() + column;
            }
        };

        // Where an exact value is in a sorted list that holds it.
        size_t IndexOf(_In_ std::vector<double> const& values, _In_ double value) noexcept
        {
            return static_cast<size_t>(std::lower_bound(values.begin(), values.end(), value) - values.begin());
        }

        bool BuildGrid(
            _Inout_ Grid& grid,
            _In_ std::vector<CordRect> const& walls,
            _In_ CordPoint const& start,
            _In_ CordPoint const& goal)
        {
            grid.Xs = { start.X, goal.X };
            grid.Ys = { start.Y, goal.Y };

            for (auto const& wall : walls)
            {
                grid.Xs.push_back(wall.Left);
                grid.Xs.push_back(wall.Right);
                grid.Ys.push_back(wall.Top);
                grid.Ys.push_back(wall.Bottom);
            }

            for (auto* values : { &grid.Xs, &grid.Ys })
            {
                std::sort(values->begin(), values->end());
                values->erase(std::unique(values->begin(), values->end()), values->end());
            }

            auto const points = grid.Xs.size() * grid.Ys.size();

            if (points > MaximumGridPoints)
            {
                return false;
            }

            grid.Blocked.assign(points, 0);
            grid.RightBlocked.assign(points, 0);
            grid.DownBlocked.assign(points, 0);

            // Inside a wall, a point is blocked, and so is a step that runs through it. A step
            // along its edge is not.
            for (auto const& wall : walls)
            {
                auto const left = IndexOf(grid.Xs, wall.Left);
                auto const right = IndexOf(grid.Xs, wall.Right);
                auto const top = IndexOf(grid.Ys, wall.Top);
                auto const bottom = IndexOf(grid.Ys, wall.Bottom);

                for (auto row = top; row <= bottom && row < grid.Ys.size(); row++)
                {
                    for (auto column = left; column <= right && column < grid.Xs.size(); column++)
                    {
                        auto const insideAcross = column > left && column < right;
                        auto const insideDown = row > top && row < bottom;
                        auto const index = grid.Index(column, row);

                        if (insideAcross && insideDown)
                        {
                            grid.Blocked[index] = 1;
                        }

                        if (insideDown && column < right)
                        {
                            grid.RightBlocked[index] = 1;
                        }

                        if (insideAcross && row < bottom)
                        {
                            grid.DownBlocked[index] = 1;
                        }
                    }
                }
            }

            return true;
        }

        // The cheapest way along the grid, counting each turn, from the start, already heading
        // right, to the goal, ready to head right. Every grid point it passes, in order.
        std::optional<std::vector<CordPoint>> SearchGrid(
            _In_ Grid const& grid,
            _In_ CordPoint const& start,
            _In_ CordPoint const& goal)
        {
            constexpr auto none = (std::numeric_limits<uint32_t>::max)();

            auto const width = grid.Xs.size();
            auto const height = grid.Ys.size();
            auto const states = width * height * HeadingCount;

            if (states >= none)
            {
                return std::nullopt;
            }

            std::vector<double> cost(states, Infinity);
            std::vector<uint32_t> previous(states, none);

            struct Open
            {
                double Estimate{ 0 };
                double Cost{ 0 };
                uint32_t State{ 0 };
            };

            // The same choice every time when two ways cost the same, so a line doesn't flip
            // between them as the canvas redraws.
            auto const later = [](Open const& a, Open const& b)
                {
                    return a.Estimate != b.Estimate ? a.Estimate > b.Estimate : a.State > b.State;
                };

            std::priority_queue<Open, std::vector<Open>, decltype(later)> open{ later };

            auto const goalIndex = grid.Index(IndexOf(grid.Xs, goal.X), IndexOf(grid.Ys, goal.Y));

            auto const remaining = [&](size_t point)
                {
                    return std::abs(grid.Xs[point % width] - goal.X) + std::abs(grid.Ys[point / width] - goal.Y);
                };

            auto const startIndex = grid.Index(IndexOf(grid.Xs, start.X), IndexOf(grid.Ys, start.Y));
            auto const first = static_cast<uint32_t>(startIndex * HeadingCount + static_cast<uint32_t>(Heading::Right));

            cost[first] = 0;
            open.push(Open{ remaining(startIndex), 0, first });

            auto best = Infinity;
            auto bestState = none;

            while (!open.empty())
            {
                auto const current = open.top();
                open.pop();

                // Nothing still waiting can do better.
                if (current.Estimate >= best)
                {
                    break;
                }

                if (current.Cost > cost[current.State])
                {
                    continue;
                }

                auto const point = current.State / HeadingCount;
                auto const heading = static_cast<Heading>(current.State % HeadingCount);

                // It can finish here unless it came in heading left, the wrong way for an In.
                if (point == goalIndex && heading != Heading::Left)
                {
                    auto const total = current.Cost + (heading == Heading::Right ? 0.0 : TurnCost);

                    if (total < best)
                    {
                        best = total;
                        bestState = current.State;
                    }

                    continue;
                }

                auto const column = point % width;
                auto const row = point / width;

                for (uint32_t next = 0; next < HeadingCount; next++)
                {
                    auto const nextHeading = static_cast<Heading>(next);

                    if (nextHeading == Reverse(heading))
                    {
                        continue;
                    }

                    size_t to{ 0 };

                    switch (nextHeading)
                    {
                    case Heading::Right:
                        if (column + 1 >= width || grid.RightBlocked[point]) continue;
                        to = point + 1;
                        break;

                    case Heading::Left:
                        if (column == 0 || grid.RightBlocked[point - 1]) continue;
                        to = point - 1;
                        break;

                    case Heading::Down:
                        if (row + 1 >= height || grid.DownBlocked[point]) continue;
                        to = point + width;
                        break;

                    case Heading::Up:
                        if (row == 0 || grid.DownBlocked[point - width]) continue;
                        to = point - width;
                        break;
                    }

                    if (grid.Blocked[to])
                    {
                        continue;
                    }

                    auto const reached = current.Cost +
                        std::abs(grid.Xs[to % width] - grid.Xs[column]) +
                        std::abs(grid.Ys[to / width] - grid.Ys[row]) +
                        (nextHeading == heading ? 0.0 : TurnCost);

                    auto const state = static_cast<uint32_t>(to * HeadingCount + next);

                    if (reached < cost[state])
                    {
                        cost[state] = reached;
                        previous[state] = current.State;
                        open.push(Open{ reached + remaining(to), reached, state });
                    }
                }
            }

            if (bestState == none)
            {
                return std::nullopt;
            }

            std::vector<CordPoint> path{};

            for (auto state = bestState; state != none; state = previous[state])
            {
                auto const point = state / HeadingCount;
                path.push_back(CordPoint{ grid.Xs[point % width], grid.Ys[point / width] });
            }

            std::reverse(path.begin(), path.end());

            return path;
        }

        // Drops repeated points, and joins runs that carry on the same way.
        void Simplify(_Inout_ std::vector<CordPoint>& points)
        {
            std::vector<CordPoint> kept{};
            kept.reserve(points.size());

            for (auto const& point : points)
            {
                if (!kept.empty() && kept.back().X == point.X && kept.back().Y == point.Y)
                {
                    continue;
                }

                if (kept.size() >= 2)
                {
                    auto const& before = kept[kept.size() - 2];
                    auto const& last = kept.back();

                    auto const sameLevel = before.Y == last.Y && last.Y == point.Y &&
                        (last.X - before.X) * (point.X - last.X) > 0;

                    auto const sameColumn = before.X == last.X && last.X == point.X &&
                        (last.Y - before.Y) * (point.Y - last.Y) > 0;

                    if (sameLevel || sameColumn)
                    {
                        kept.back() = point;
                        continue;
                    }
                }

                kept.push_back(point);
            }

            points = std::move(kept);
        }

        // The runs from the Out to the In, around the walls, as the points where the line turns.
        // It starts with the walls near the two ends. Any other wall the way it finds runs into is
        // added, and it looks again, so a big canvas doesn't make every line slow.
        std::optional<std::vector<CordPoint>> FindRuns(
            _In_ CordPoint const& from,
            _In_ CordPoint const& to,
            _In_ CordPoint const& start,
            _In_ CordPoint const& goal,
            _In_ std::vector<std::optional<CordRect>> const& walls)
        {
            auto const margin = Lead + Comfort;

            CordRect const nearEnds{
                (std::min)(from.X, to.X) - margin,
                (std::min)(from.Y, to.Y) - margin,
                (std::max)(from.X, to.X) + margin,
                (std::max)(from.Y, to.Y) + margin };

            std::vector<uint8_t> chosen(walls.size(), 0);

            for (size_t i = 0; i < walls.size(); i++)
            {
                if (walls[i].has_value() && Overlaps(walls[i].value(), nearEnds))
                {
                    chosen[i] = 1;
                }
            }

            for (int round = 0; round < MaximumRounds; round++)
            {
                std::vector<CordRect> inUse{};

                for (size_t i = 0; i < walls.size(); i++)
                {
                    if (chosen[i] != 0)
                    {
                        inUse.push_back(walls[i].value());
                    }
                }

                if (inUse.size() > MaximumWalls)
                {
                    return std::nullopt;
                }

                Grid grid{};

                if (!BuildGrid(grid, inUse, start, goal))
                {
                    return std::nullopt;
                }

                auto found = SearchGrid(grid, start, goal);

                if (!found.has_value())
                {
                    return std::nullopt;
                }

                Simplify(found.value());

                auto added = false;

                for (size_t i = 0; i < walls.size(); i++)
                {
                    if (chosen[i] != 0 || !walls[i].has_value())
                    {
                        continue;
                    }

                    for (size_t point = 1; point < found->size(); point++)
                    {
                        if (CrossesInside((*found)[point - 1], (*found)[point], walls[i].value()))
                        {
                            chosen[i] = 1;
                            added = true;
                            break;
                        }
                    }
                }

                if (!added)
                {
                    std::vector<CordPoint> points{ from };
                    points.insert(points.end(), found->begin(), found->end());
                    points.push_back(to);

                    Simplify(points);

                    return points;
                }
            }

            return std::nullopt;
        }

        // How far run k of a line can move across itself: as far as the walls along it allow, and
        // the runs either side, which keep going the way they go now and stay long enough to turn.
        // The first and last runs come out of the Out and go into the In, so they can't move.
        struct Room
        {
            bool Level{ false };
            double At{ 0 };
            double Low{ 0 };
            double High{ 0 };

            // Where the runs either side of it start and end.
            double Before{ 0 };
            double After{ 0 };
        };

        std::optional<Room> RoomFor(
            _In_ std::vector<CordPoint> const& points,
            _In_ size_t k,
            _In_ std::vector<std::optional<CordRect>> const& walls) noexcept
        {
            auto const count = points.size();

            if (k == 0 || k + 2 >= count)
            {
                return std::nullopt;
            }

            auto const& a = points[k];
            auto const& b = points[k + 1];

            Room room{};
            room.Level = a.Y == b.Y;
            room.At = room.Level ? a.Y : a.X;
            room.Before = room.Level ? points[k - 1].Y : points[k - 1].X;
            room.After = room.Level ? points[k + 2].Y : points[k + 2].X;

            auto const spanLow = room.Level ? (std::min)(a.X, b.X) : (std::min)(a.Y, b.Y);
            auto const spanHigh = room.Level ? (std::max)(a.X, b.X) : (std::max)(a.Y, b.Y);

            auto const beforeLength = k == 1 ? Lead : MinimumRun;
            auto const afterLength = k + 2 == count - 1 ? Lead : MinimumRun;

            auto low = -Infinity;
            auto high = Infinity;

            if (room.Before < room.At)
            {
                low = (std::max)(low, room.Before + beforeLength);
            }
            else
            {
                high = (std::min)(high, room.Before - beforeLength);
            }

            if (room.After > room.At)
            {
                high = (std::min)(high, room.After - afterLength);
            }
            else
            {
                low = (std::max)(low, room.After + afterLength);
            }

            for (auto const& wall : walls)
            {
                if (!wall.has_value())
                {
                    continue;
                }

                auto const wallLow = room.Level ? wall->Left : wall->Top;
                auto const wallHigh = room.Level ? wall->Right : wall->Bottom;
                auto const wallNear = room.Level ? wall->Top : wall->Left;
                auto const wallFar = room.Level ? wall->Bottom : wall->Right;

                if (!(wallLow < spanHigh && wallHigh > spanLow))
                {
                    continue;
                }

                if (wallFar <= room.At)
                {
                    low = (std::max)(low, wallFar);
                }
                else if (wallNear >= room.At)
                {
                    high = (std::min)(high, wallNear);
                }
                else
                {
                    // Inside a wall already. Leave it where it is.
                    return std::nullopt;
                }
            }

            if (!(low <= high))
            {
                return std::nullopt;
            }

            room.Low = low;
            room.High = high;

            return room;
        }

        void MoveRun(_Inout_ std::vector<CordPoint>& points, _In_ size_t k, _In_ bool level, _In_ double to) noexcept
        {
            if (level)
            {
                points[k].Y = to;
                points[k + 1].Y = to;
            }
            else
            {
                points[k].X = to;
                points[k + 1].X = to;
            }
        }

        // Moves each run between two turns to where it looks best. A step from one level to
        // another goes halfway across the room it has. A run that doubles back around something
        // keeps a comfortable distance from it, instead of hugging it.
        void Tidy(_Inout_ std::vector<CordPoint>& points, _In_ std::vector<std::optional<CordRect>> const& walls) noexcept
        {
            for (int pass = 0; pass < TidyPasses; pass++)
            {
                for (size_t k = 1; k + 2 < points.size(); k++)
                {
                    auto const room = RoomFor(points, k, walls);

                    if (!room.has_value())
                    {
                        continue;
                    }

                    auto const span = room->High - room->Low;
                    double target{ room->At };

                    if ((room->Before < room->At) == (room->After > room->At))
                    {
                        target = (room->Low + room->High) / 2;
                    }
                    else if (room->Before < room->At)
                    {
                        target = room->Low + (std::min)(Comfort - Clearance, span / 2);
                    }
                    else
                    {
                        target = room->High - (std::min)(Comfort - Clearance, span / 2);
                    }

                    if (std::isfinite(target))
                    {
                        MoveRun(points, k, room->Level, target);
                    }
                }
            }
        }

        // Whether a rounded turn keeps clear of every node. Where the line leaves its source or
        // arrives at its destination, only that node's own edge counts, since the line touches it.
        bool TurnIsClear(
            _In_ CordPoint const& enter,
            _In_ CordPiece const& turn,
            _In_ std::vector<CordRect> const& nodes,
            _In_ std::vector<std::optional<CordRect>> const& walls,
            _In_ size_t touchingSource,
            _In_ size_t touchingDestination)
        {
            CordPoint samples[TurnCheckSamples + 1]{};

            samples[0] = enter;

            for (int i = 1; i <= TurnCheckSamples; i++)
            {
                samples[i] = PointOnPiece(enter, turn, static_cast<double>(i) / TurnCheckSamples);
            }

            CordRect box{
                (std::min)({ enter.X, turn.Control1.X, turn.Control2.X, turn.End.X }),
                (std::min)({ enter.Y, turn.Control1.Y, turn.Control2.Y, turn.End.Y }),
                (std::max)({ enter.X, turn.Control1.X, turn.Control2.X, turn.End.X }),
                (std::max)({ enter.Y, turn.Control1.Y, turn.Control2.Y, turn.End.Y }) };

            for (size_t node = 0; node < nodes.size(); node++)
            {
                // Left out of the way the line was found, so left out here too.
                if (!walls[node].has_value())
                {
                    continue;
                }

                auto const area = node == touchingSource || node == touchingDestination
                    ? Grown(nodes[node], -EndTolerance)
                    : Grown(nodes[node], CurveClearance);

                if (!Overlaps(area, box))
                {
                    continue;
                }

                for (int i = 1; i <= TurnCheckSamples; i++)
                {
                    if (CrossesInside(samples[i - 1], samples[i], area))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        // Where a label could go on a line, best first: along its straight runs other than the two
        // at its ends, longest first, from the middle of each run outward.
        std::vector<CordPoint> LabelSpots(_In_ CordPath const& path)
        {
            struct StraightRun
            {
                double Score{ 0 };
                CordPoint From{};
                CordPoint To{};
            };

            std::vector<StraightRun> runs{};
            auto from = path.Start;

            for (size_t i = 0; i < path.Pieces.size(); i++)
            {
                auto const& piece = path.Pieces[i];

                if (piece.IsStraight && i > 0 && i + 1 < path.Pieces.size())
                {
                    runs.push_back(StraightRun{
                        Distance(from, piece.End) * (from.Y == piece.End.Y ? LevelLabelPreference : 1.0),
                        from,
                        piece.End });
                }

                from = piece.End;
            }

            std::stable_sort(runs.begin(), runs.end(),
                [](StraightRun const& a, StraightRun const& b) { return a.Score > b.Score; });

            std::vector<CordPoint> spots{};

            for (auto const& run : runs)
            {
                for (auto const fraction : LabelFractions)
                {
                    spots.push_back(CordPoint{
                        run.From.X + (run.To.X - run.From.X) * fraction,
                        run.From.Y + (run.To.Y - run.From.Y) * fraction });
                }
            }

            return spots;
        }

        CordPoint HalfwayAlong(_In_ CordPath const& path)
        {
            auto const samples = SampleCordPath(path, CheckSamples);

            auto total = 0.0;

            for (size_t i = 1; i < samples.size(); i++)
            {
                total += Distance(samples[i - 1], samples[i]);
            }

            auto walked = 0.0;

            for (size_t i = 1; i < samples.size(); i++)
            {
                auto const step = Distance(samples[i - 1], samples[i]);

                if (step > 0 && walked + step >= total / 2)
                {
                    auto const t = (total / 2 - walked) / step;

                    return CordPoint{
                        samples[i - 1].X + (samples[i].X - samples[i - 1].X) * t,
                        samples[i - 1].Y + (samples[i].Y - samples[i - 1].Y) * t };
                }

                walked += step;
            }

            return path.Start;
        }

        // Straight runs, with each turn rounded off as far as the room around it allows.
        CordPath RoundTurns(
            _In_ std::vector<CordPoint> const& points,
            _In_ std::vector<CordRect> const& nodes,
            _In_ std::vector<std::optional<CordRect>> const& walls,
            _In_ size_t source,
            _In_ size_t destination)
        {
            CordPath path{};
            path.Start = points.front();
            path.GoesAround = true;

            auto const last = points.size() - 1;
            auto at = points.front();

            for (size_t k = 1; k < last; k++)
            {
                auto const& corner = points[k];
                auto const in = Direction(points[k - 1], corner);
                auto const out = Direction(corner, points[k + 1]);

                // A run between two turns gives each of them half of itself. The runs out of the
                // Out and into the In have only one turn each.
                auto const inLength = Distance(points[k - 1], corner);
                auto const outLength = Distance(corner, points[k + 1]);

                auto inRadius = (std::min)(TurnRadius, k == 1 ? inLength : inLength / 2);
                auto outRadius = (std::min)(TurnRadius, k + 1 == last ? outLength : outLength / 2);

                std::optional<std::pair<CordPoint, CordPiece>> turn{};

                for (int attempt = 0; attempt <= TurnShrinkSteps; attempt++)
                {
                    CordPoint const enter{ corner.X - in.X * inRadius, corner.Y - in.Y * inRadius };
                    CordPoint const leave{ corner.X + out.X * outRadius, corner.Y + out.Y * outRadius };

                    CordPiece const piece{
                        CordPoint{ enter.X + in.X * inRadius * QuarterEllipse, enter.Y + in.Y * inRadius * QuarterEllipse },
                        CordPoint{ leave.X - out.X * outRadius * QuarterEllipse, leave.Y - out.Y * outRadius * QuarterEllipse },
                        leave,
                        false };

                    if (TurnIsClear(enter, piece, nodes, walls,
                        k == 1 ? source : NoNode,
                        k + 1 == last ? destination : NoNode))
                    {
                        turn.emplace(enter, piece);
                        break;
                    }

                    inRadius *= TurnShrink;
                    outRadius *= TurnShrink;
                }

                if (!turn.has_value())
                {
                    // No room to round it off, so the line just turns.
                    AppendStraight(path, at, corner);
                    at = corner;
                    continue;
                }

                AppendStraight(path, at, turn->first);
                path.Pieces.push_back(turn->second);
                at = turn->second.End;
            }

            AppendStraight(path, at, points.back());

            auto const spots = LabelSpots(path);
            path.LabelPoint = spots.empty() ? HalfwayAlong(path) : spots.front();

            return path;
        }

        // One line, before the lines that go around things are spread apart and rounded off.
        struct Planned
        {
            CordPath Path{};

            // It goes around something: these are its runs, and the room each node takes up.
            bool Routed{ false };
            std::vector<CordPoint> Points{};
            std::vector<std::optional<CordRect>> Walls{};
        };

        Planned PlanOne(_In_ CordRequest const& request, _In_ std::vector<CordRect> const& nodes)
        {
            Planned planned{};

            auto const& from = request.From;
            auto const& to = request.To;

            if (!IsFinite(from) || !IsFinite(to))
            {
                planned.Path.Start = from;
                planned.Path.LabelPoint = from;
                return planned;
            }

            auto const forward = to.X - from.X >= ForwardDistance;

            if (forward)
            {
                planned.Path = UsualCurve(from, to);

                if (IsClear(planned.Path, nodes, request.SourceNode, request.DestinationNode))
                {
                    return planned;
                }
            }

            CordPoint const start{ from.X + Lead, from.Y };
            CordPoint const goal{ to.X - Lead, to.Y };

            planned.Walls.resize(nodes.size());

            for (size_t i = 0; i < nodes.size(); i++)
            {
                if (IsFinite(nodes[i]))
                {
                    planned.Walls[i] = WallAround(nodes[i], start, goal);
                }
            }

            // After the nodes' own, so a node's index is still its wall's.
            for (auto const& edge : OffCanvas)
            {
                planned.Walls.push_back(WallAround(edge, start, goal));
            }

            auto points = FindRuns(from, to, start, goal, planned.Walls);

            if (!points.has_value() || points->size() < 2)
            {
                planned.Walls.clear();
                planned.Path = forward ? UsualCurve(from, to) : LoopUnder(from, to);
                return planned;
            }

            Tidy(points.value(), planned.Walls);

            planned.Routed = true;
            planned.Points = std::move(points.value());

            return planned;
        }

        // Lines that go around things often find the same way past them. Where runs of two lines
        // would be drawn over one another, they're moved apart, as far as each has room. A run out
        // of an Out or into an In can't move, so the others make room around it.
        void SpreadSharedRuns(_Inout_ std::vector<Planned>& planned)
        {
            struct Run
            {
                size_t Line{ 0 };
                size_t Segment{ 0 };
                bool Level{ false };
                bool Fixed{ false };
                double At{ 0 };
                double SpanLow{ 0 };
                double SpanHigh{ 0 };

                // Where the runs either side head, on average. A run whose line comes from and
                // goes on above another's goes above it, so they don't cross.
                double Toward{ 0 };
            };

            std::vector<Run> runs{};

            for (size_t line = 0; line < planned.size(); line++)
            {
                if (!planned[line].Routed)
                {
                    continue;
                }

                auto const& points = planned[line].Points;
                auto const count = points.size();

                for (size_t k = 0; k + 1 < count; k++)
                {
                    auto const& a = points[k];
                    auto const& b = points[k + 1];

                    Run run{};
                    run.Line = line;
                    run.Segment = k;
                    run.Level = a.Y == b.Y;
                    run.Fixed = k == 0 || k + 2 == count;
                    run.At = run.Level ? a.Y : a.X;
                    run.SpanLow = run.Level ? (std::min)(a.X, b.X) : (std::min)(a.Y, b.Y);
                    run.SpanHigh = run.Level ? (std::max)(a.X, b.X) : (std::max)(a.Y, b.Y);

                    auto const before = k > 0 ? (run.Level ? points[k - 1].Y : points[k - 1].X) : run.At;
                    auto const after = k + 2 < count ? (run.Level ? points[k + 2].Y : points[k + 2].X) : run.At;

                    run.Toward = (before + after) / 2;

                    runs.push_back(run);
                }
            }

            if (runs.size() < 2 || runs.size() > MaximumSpreadRuns)
            {
                return;
            }

            std::vector<size_t> order(runs.size());
            std::iota(order.begin(), order.end(), size_t{ 0 });

            std::sort(order.begin(), order.end(), [&runs](size_t a, size_t b)
                {
                    if (runs[a].Level != runs[b].Level) return runs[a].Level < runs[b].Level;
                    if (runs[a].At != runs[b].At) return runs[a].At < runs[b].At;
                    return a < b;
                });

            // Runs that share a stretch closer together than the spacing end up in one group.
            std::vector<size_t> group(runs.size());
            std::iota(group.begin(), group.end(), size_t{ 0 });

            auto const rootOf = [&group](size_t i)
                {
                    while (group[i] != i)
                    {
                        group[i] = group[group[i]];
                        i = group[i];
                    }

                    return i;
                };

            for (size_t i = 0; i < order.size(); i++)
            {
                auto const& a = runs[order[i]];

                for (size_t j = i + 1; j < order.size(); j++)
                {
                    auto const& b = runs[order[j]];

                    if (b.Level != a.Level || b.At - a.At >= RunSpacing)
                    {
                        break;
                    }

                    auto const shared = (std::min)(a.SpanHigh, b.SpanHigh) - (std::max)(a.SpanLow, b.SpanLow);

                    if (a.Line != b.Line && shared > RunSpacing)
                    {
                        group[rootOf(order[i])] = rootOf(order[j]);
                    }
                }
            }

            std::map<size_t, std::vector<size_t>> groups{};

            for (size_t i = 0; i < runs.size(); i++)
            {
                groups[rootOf(i)].push_back(i);
            }

            for (auto& [root, members] : groups)
            {
                if (members.size() < 2 ||
                    std::all_of(members.begin(), members.end(), [&runs](size_t i) { return runs[i].Fixed; }))
                {
                    continue;
                }

                std::sort(members.begin(), members.end(), [&runs](size_t a, size_t b)
                    {
                        if (runs[a].Toward != runs[b].Toward) return runs[a].Toward < runs[b].Toward;
                        return a < b;
                    });

                auto const count = members.size();

                auto center = 0.0;

                for (auto const i : members)
                {
                    center += runs[i].At;
                }

                center /= static_cast<double>(count);

                std::vector<double> low(count);
                std::vector<double> high(count);
                std::vector<double> position(count);

                for (size_t i = 0; i < count; i++)
                {
                    auto const& run = runs[members[i]];
                    auto const room = run.Fixed
                        ? std::nullopt
                        : RoomFor(planned[run.Line].Points, run.Segment, planned[run.Line].Walls);

                    low[i] = room.has_value() ? room->Low : run.At;
                    high[i] = room.has_value() ? room->High : run.At;
                    position[i] = center + (static_cast<double>(i) - static_cast<double>(count - 1) / 2) * RunSpacing;
                }

                // Evenly spaced around where they were, then pushed apart within the room each has.
                position[0] = std::clamp(position[0], low[0], high[0]);

                for (size_t i = 1; i < count; i++)
                {
                    position[i] = std::clamp((std::max)(position[i], position[i - 1] + RunSpacing), low[i], high[i]);
                }

                for (size_t i = count - 1; i-- > 0;)
                {
                    position[i] = std::clamp((std::min)(position[i], position[i + 1] - RunSpacing), low[i], high[i]);
                }

                for (size_t i = 0; i < count; i++)
                {
                    auto const& run = runs[members[i]];

                    if (run.Fixed)
                    {
                        continue;
                    }

                    // Measured again: another run of the same line may have moved since.
                    auto& line = planned[run.Line];

                    if (auto const room = RoomFor(line.Points, run.Segment, line.Walls); room.has_value())
                    {
                        MoveRun(line.Points, run.Segment, room->Level, std::clamp(position[i], room->Low, room->High));
                    }
                }
            }
        }

        CordRect LabelArea(_In_ CordPoint const& center) noexcept
        {
            return CordRect{
                center.X - LabelWidth / 2,
                center.Y - LabelHeight / 2,
                center.X + LabelWidth / 2,
                center.Y + LabelHeight / 2 };
        }

        // Labels on lines that go around things move along their lines, where they can, to clear
        // the labels already placed. Labels on the usual curves stay where they always were.
        void PlaceLabels(_Inout_ std::vector<CordPath>& paths, _In_ std::vector<CordRequest> const& requests)
        {
            std::vector<CordRect> placed{};

            for (size_t i = 0; i < paths.size(); i++)
            {
                if (requests[i].HasLabel && !paths[i].GoesAround)
                {
                    placed.push_back(LabelArea(paths[i].LabelPoint));
                }
            }

            for (size_t i = 0; i < paths.size(); i++)
            {
                if (!requests[i].HasLabel || !paths[i].GoesAround)
                {
                    continue;
                }

                for (auto const& spot : LabelSpots(paths[i]))
                {
                    auto const area = LabelArea(spot);

                    if (std::none_of(placed.begin(), placed.end(),
                        [&area](CordRect const& other) { return Overlaps(area, other); }))
                    {
                        paths[i].LabelPoint = spot;
                        break;
                    }
                }

                placed.push_back(LabelArea(paths[i].LabelPoint));
            }
        }
    }

    _Use_decl_annotations_
    std::vector<CordPoint> SampleCordPath(CordPath const& path, int samplesPerCurve) noexcept
    {
        std::vector<CordPoint> samples{};

        try
        {
            auto const steps = (std::max)(samplesPerCurve, 1);

            samples.reserve(1 + path.Pieces.size() * static_cast<size_t>(steps));
            samples.push_back(path.Start);

            auto start = path.Start;

            for (auto const& piece : path.Pieces)
            {
                if (piece.IsStraight)
                {
                    samples.push_back(piece.End);
                }
                else
                {
                    for (int i = 1; i <= steps; i++)
                    {
                        samples.push_back(PointOnPiece(start, piece, static_cast<double>(i) / steps));
                    }
                }

                start = piece.End;
            }
        }
        catch (...)
        {
            samples.clear();
        }

        return samples;
    }

    _Use_decl_annotations_
    CordPath UsualCordPath(CordPoint const& from, CordPoint const& to) noexcept
    {
        try
        {
            return UsualCurve(from, to);
        }
        catch (...)
        {
        }

        CordPath none{};
        none.Start = from;
        none.LabelPoint = from;

        return none;
    }

    _Use_decl_annotations_
    std::vector<CordPath> PlanCordPaths(std::vector<CordRequest> const& requests, std::vector<CordRect> const& nodes) noexcept
    {
        try
        {
            std::vector<Planned> planned{};
            planned.reserve(requests.size());

            for (auto const& request : requests)
            {
                planned.push_back(PlanOne(request, nodes));
            }

            SpreadSharedRuns(planned);

            std::vector<CordPath> paths{};
            paths.reserve(planned.size());

            for (size_t i = 0; i < planned.size(); i++)
            {
                if (planned[i].Routed)
                {
                    paths.push_back(RoundTurns(planned[i].Points, nodes, planned[i].Walls,
                        requests[i].SourceNode, requests[i].DestinationNode));
                }
                else
                {
                    paths.push_back(std::move(planned[i].Path));
                }
            }

            PlaceLabels(paths, requests);

            return paths;
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    CordPath PlanCordPath(
        CordPoint const& from,
        CordPoint const& to,
        std::vector<CordRect> const& nodes,
        size_t sourceNode,
        size_t destinationNode) noexcept
    {
        try
        {
            auto paths = PlanCordPaths({ CordRequest{ from, to, sourceNode, destinationNode } }, nodes);

            if (paths.size() == 1)
            {
                return std::move(paths.front());
            }
        }
        catch (...)
        {
        }

        // Nothing to draw. Making even the usual curve needs memory, which is what just ran out.
        CordPath none{};
        none.Start = from;
        none.LabelPoint = from;

        return none;
    }
}
