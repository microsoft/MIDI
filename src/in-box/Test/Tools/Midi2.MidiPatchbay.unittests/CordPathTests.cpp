// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "CordPathTests.h"

#include "CordPath.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vector>

using namespace midipatchbay;
using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    // From a node's Out at a height to another node's In at a height.
    struct Link
    {
        size_t From{ 0 };
        double FromY{ 0 };
        size_t To{ 0 };
        double ToY{ 0 };
    };

    struct Layout
    {
        std::vector<CordRect> Nodes{};
        std::vector<Link> Links{};
    };

    CordRequest RequestFor(_In_ Layout const& layout, _In_ Link const& link)
    {
        return CordRequest{
            CordPoint{ layout.Nodes[link.From].Right, link.FromY },
            CordPoint{ layout.Nodes[link.To].Left, link.ToY },
            link.From,
            link.To,
            true };
    }

    std::vector<CordPath> Plan(_In_ Layout const& layout)
    {
        std::vector<CordRequest> requests{};

        for (auto const& link : layout.Links)
        {
            requests.push_back(RequestFor(layout, link));
        }

        return PlanCordPaths(requests, layout.Nodes);
    }

    // Points along the line no more than a pixel apart, worked out here rather than trusted to
    // the code under test, so a straight run through a node can't hide between two samples.
    std::vector<CordPoint> Trace(_In_ CordPath const& path)
    {
        auto const coarse = SampleCordPath(path, 64);
        std::vector<CordPoint> fine{};

        for (size_t i = 0; i < coarse.size(); i++)
        {
            if (i == 0)
            {
                fine.push_back(coarse[i]);
                continue;
            }

            auto const& a = coarse[i - 1];
            auto const& b = coarse[i];
            auto const steps = (std::max)(1, static_cast<int>(std::ceil(std::hypot(b.X - a.X, b.Y - a.Y))));

            for (int step = 1; step <= steps; step++)
            {
                auto const t = static_cast<double>(step) / steps;
                fine.push_back(CordPoint{ a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t });
            }
        }

        return fine;
    }

    // How many nodes the line goes through. A line touches the edge of its own nodes at its ends,
    // so a pixel in from every edge is what counts.
    size_t NodesCrossed(_In_ CordPath const& path, _In_ std::vector<CordRect> const& nodes)
    {
        auto const points = Trace(path);
        size_t crossed{ 0 };

        for (auto const& node : nodes)
        {
            auto const inside = std::any_of(points.begin(), points.end(), [&node](CordPoint const& p)
                {
                    return p.X > node.Left + 1 && p.X < node.Right - 1 && p.Y > node.Top + 1 && p.Y < node.Bottom - 1;
                });

            crossed += inside ? 1 : 0;
        }

        return crossed;
    }

    CordPoint Unit(_In_ double x, _In_ double y)
    {
        auto const length = std::hypot(x, y);

        return length > 1e-9 ? CordPoint{ x / length, y / length } : CordPoint{};
    }

    // Which way a piece sets off, and which way it arrives.
    CordPoint Leaving(_In_ CordPoint const& start, _In_ CordPiece const& piece)
    {
        for (auto const& toward : { piece.Control1, piece.Control2, piece.End })
        {
            if (std::hypot(toward.X - start.X, toward.Y - start.Y) > 1e-9)
            {
                return Unit(toward.X - start.X, toward.Y - start.Y);
            }
        }

        return {};
    }

    CordPoint Arriving(_In_ CordPoint const& start, _In_ CordPiece const& piece)
    {
        for (auto const& from : { piece.Control2, piece.Control1, start })
        {
            if (std::hypot(piece.End.X - from.X, piece.End.Y - from.Y) > 1e-9)
            {
                return Unit(piece.End.X - from.X, piece.End.Y - from.Y);
            }
        }

        return {};
    }

    double DegreesBetween(_In_ CordPoint const& a, _In_ CordPoint const& b)
    {
        auto const dot = std::clamp(a.X * b.X + a.Y * b.Y, -1.0, 1.0);

        return std::acos(dot) * 180.0 / 3.14159265358979323846;
    }

    double DistanceToLine(_In_ CordPoint const& point, _In_ CordPath const& path)
    {
        auto nearest = std::numeric_limits<double>::infinity();

        for (auto const& p : Trace(path))
        {
            nearest = (std::min)(nearest, std::hypot(p.X - point.X, p.Y - point.Y));
        }

        return nearest;
    }

    // Nodes at least 40 pixels apart, the sizes endpoints and steps have, and lines between
    // random rows, forward and back.
    Layout RandomLayout(_In_ unsigned seed)
    {
        std::mt19937 random{ seed };
        std::uniform_real_distribution<double> x(0, 1400);
        std::uniform_real_distribution<double> y(0, 900);

        Layout layout{};

        for (int attempt = 0; attempt < 2000 && layout.Nodes.size() < 14; attempt++)
        {
            auto const step = random() % 2 == 0;
            auto const width = step ? 208.0 : 280.0;
            auto const height = step ? 99.0 : 130.0 + (random() % 3) * 32.0;

            CordRect node{ std::round(x(random)), std::round(y(random)), 0, 0 };
            node.Right = node.Left + width;
            node.Bottom = node.Top + height;

            auto const apart = std::none_of(layout.Nodes.begin(), layout.Nodes.end(), [&node](CordRect const& other)
                {
                    return node.Left < other.Right + 40 && other.Left < node.Right + 40 &&
                        node.Top < other.Bottom + 40 && other.Top < node.Bottom + 40;
                });

            if (apart)
            {
                layout.Nodes.push_back(node);
            }
        }

        for (int i = 0; i < 16; i++)
        {
            auto const from = random() % layout.Nodes.size();
            auto const to = random() % layout.Nodes.size();

            auto const& a = layout.Nodes[from];
            auto const& b = layout.Nodes[to];

            layout.Links.push_back(Link{
                from, a.Top + 50 + random() % static_cast<unsigned>(a.Bottom - a.Top - 60),
                to, b.Top + 50 + random() % static_cast<unsigned>(b.Bottom - b.Top - 60) });
        }

        return layout;
    }

    // The canvas from the report that started this: a backward line from Clock into the In of
    // OXI ONE, below and to the right of it, among a few other devices.
    Layout ReportedLayout()
    {
        Layout layout{};
        layout.Nodes = {
            CordRect{ 0, 128, 265, 280 },       // Clock
            CordRect{ 143, 415, 410, 640 },     // OXI ONE MKII
            CordRect{ 677, -30, 900, 150 },
            CordRect{ 697, 185, 900, 337 },
            CordRect{ 715, 380, 900, 565 },
            CordRect{ 765, 680, 960, 780 },
        };
        layout.Links = {
            Link{ 0, 256, 2, 60 },
            Link{ 0, 256, 3, 314 },
            Link{ 0, 256, 4, 544 },
            Link{ 0, 256, 1, 544 },
            Link{ 0, 256, 5, 744 },
        };

        return layout;
    }

    // The longest level straight run of a line, other than where it leaves and arrives.
    std::optional<std::pair<double, std::pair<double, double>>> LongestLevelRun(_In_ CordPath const& path)
    {
        std::optional<std::pair<double, std::pair<double, double>>> best{};
        auto bestLength = 0.0;
        auto start = path.Start;

        for (size_t i = 0; i < path.Pieces.size(); i++)
        {
            auto const& piece = path.Pieces[i];

            if (piece.IsStraight && start.Y == piece.End.Y && i > 0 && i + 1 < path.Pieces.size() &&
                std::abs(piece.End.X - start.X) > bestLength)
            {
                bestLength = std::abs(piece.End.X - start.X);
                best = std::pair{ start.Y, std::pair{ (std::min)(start.X, piece.End.X), (std::max)(start.X, piece.End.X) } };
            }

            start = piece.End;
        }

        return best;
    }
}

void CordPathTests::AClearLineKeepsTheUsualCurve()
{
    std::vector<CordRect> const nodes{ CordRect{ 48, 50, 300, 200 }, CordRect{ 600, 150, 880, 300 } };

    auto const path = PlanCordPath(CordPoint{ 300, 150 }, CordPoint{ 600, 250 }, nodes, 0, 1);

    VERIFY_IS_FALSE(path.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 1 }, path.Pieces.size());

    // The same curve earlier versions drew: its ends reach out 0.55 of the way across, at most 140.
    VERIFY_ARE_EQUAL(440.0, path.Pieces[0].Control1.X);
    VERIFY_ARE_EQUAL(150.0, path.Pieces[0].Control1.Y);
    VERIFY_ARE_EQUAL(460.0, path.Pieces[0].Control2.X);
    VERIFY_ARE_EQUAL(250.0, path.Pieces[0].Control2.Y);
    VERIFY_ARE_EQUAL(600.0, path.Pieces[0].End.X);
    VERIFY_ARE_EQUAL(250.0, path.Pieces[0].End.Y);
    VERIFY_ARE_EQUAL(450.0, path.LabelPoint.X);
    VERIFY_ARE_EQUAL(200.0, path.LabelPoint.Y);
}

void CordPathTests::ALineBackToANodeBelowGoesOverItsTop()
{
    auto const layout = ReportedLayout();
    auto const paths = Plan(layout);

    VERIFY_ARE_EQUAL(layout.Links.size(), paths.size());

    auto const& path = paths[3];
    auto const& oxi = layout.Nodes[1];

    VERIFY_IS_TRUE(path.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 0 }, NodesCrossed(path, layout.Nodes));

    auto const points = Trace(path);

    // Never lower than the In it arrives at, so it came over the top and not around the bottom.
    VERIFY_IS_TRUE(std::all_of(points.begin(), points.end(), [](CordPoint const& p) { return p.Y <= 544.0 + 0.01; }));

    // Above the node wherever it's over it.
    VERIFY_IS_TRUE(std::all_of(points.begin(), points.end(), [&oxi](CordPoint const& p) { return p.X <= oxi.Left + 1 || p.Y < oxi.Top; }));

    // The lines that had a clear way still take the usual curve.
    for (auto const i : { size_t{ 0 }, size_t{ 1 }, size_t{ 2 }, size_t{ 4 } })
    {
        VERIFY_IS_FALSE(paths[i].GoesAround);
        VERIFY_ARE_EQUAL(size_t{ 1 }, paths[i].Pieces.size());
    }
}

void CordPathTests::ALineBackToItsOwnNodeGoesAroundIt()
{
    std::vector<CordRect> const nodes{ CordRect{ 100, 100, 352, 262 } };

    auto const path = PlanCordPath(CordPoint{ 352, 182 }, CordPoint{ 100, 182 }, nodes, 0, 0);

    VERIFY_IS_TRUE(path.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 0 }, NodesCrossed(path, nodes));

    auto const points = Trace(path);

    VERIFY_IS_TRUE(std::any_of(points.begin(), points.end(), [](CordPoint const& p) { return p.X > 352 + 12; }));
    VERIFY_IS_TRUE(std::any_of(points.begin(), points.end(), [](CordPoint const& p) { return p.X < 100 - 12; }));
    VERIFY_IS_TRUE(std::any_of(points.begin(), points.end(), [](CordPoint const& p) { return p.Y > 262 + 12 || p.Y < 100 - 12; }));
}

void CordPathTests::ALineStaysOnTheCanvas()
{
    // Over the top would be shorter, but there isn't room above the node, and nothing left of the
    // canvas or above it can be scrolled to.
    std::vector<CordRect> const nodes{ CordRect{ 20, 20, 272, 182 } };

    auto const path = PlanCordPath(CordPoint{ 272, 60 }, CordPoint{ 20, 60 }, nodes, 0, 0);

    VERIFY_IS_TRUE(path.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 0 }, NodesCrossed(path, nodes));

    auto const points = Trace(path);

    VERIFY_IS_TRUE(std::all_of(points.begin(), points.end(), [](CordPoint const& p) { return p.X >= 0 && p.Y >= 0; }));
    VERIFY_IS_TRUE(std::any_of(points.begin(), points.end(), [](CordPoint const& p) { return p.Y > 182; }));
}

void CordPathTests::SomethingInTheWayIsGoneAround()
{
    std::vector<CordRect> const nodes{
        CordRect{ 0, 100, 252, 230 },       // keyboard
        CordRect{ 340, 90, 548, 189 },      // a step, right in the way
        CordRect{ 640, 100, 892, 230 },     // synth, level with the keyboard
        CordRect{ 640, 300, 892, 430 },     // drums, lower down with nothing in the way
    };

    auto const blocked = PlanCordPath(CordPoint{ 252, 182 }, CordPoint{ 640, 182 }, nodes, 0, 2);

    VERIFY_IS_TRUE(blocked.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 0 }, NodesCrossed(blocked, nodes));

    auto const clear = PlanCordPath(CordPoint{ 252, 214 }, CordPoint{ 640, 362 }, nodes, 0, 3);

    VERIFY_IS_FALSE(clear.GoesAround);
    VERIFY_ARE_EQUAL(size_t{ 1 }, clear.Pieces.size());
}

void CordPathTests::NoLineCrossesANode()
{
    size_t lines{ 0 };
    size_t around{ 0 };

    for (unsigned seed = 100; seed < 220; seed++)
    {
        auto const layout = RandomLayout(seed);
        auto const paths = Plan(layout);

        VERIFY_ARE_EQUAL(layout.Links.size(), paths.size());

        for (size_t i = 0; i < paths.size(); i++)
        {
            lines++;
            around += paths[i].GoesAround ? 1 : 0;

            auto const crossed = NodesCrossed(paths[i], layout.Nodes);

            if (crossed != 0)
            {
                Log::Comment(String().Format(L"Layout %u, line %zu crosses %zu nodes", seed, i, crossed));
            }

            VERIFY_ARE_EQUAL(size_t{ 0 }, crossed);

            // A line back to an earlier node always finds a way around rather than falling back
            // to the loop earlier versions drew.
            if (layout.Nodes[layout.Links[i].To].Left < layout.Nodes[layout.Links[i].From].Right + 20)
            {
                VERIFY_IS_TRUE(paths[i].GoesAround);
            }
        }
    }

    Log::Comment(String().Format(L"%zu lines, %zu went around something", lines, around));
}

void CordPathTests::LinesAreSmoothAndMeetTheirPointsLevel()
{
    for (unsigned seed = 300; seed < 360; seed++)
    {
        auto const layout = RandomLayout(seed);
        auto const paths = Plan(layout);

        VERIFY_ARE_EQUAL(layout.Links.size(), paths.size());

        for (size_t i = 0; i < paths.size(); i++)
        {
            auto const& path = paths[i];
            auto const request = RequestFor(layout, layout.Links[i]);

            VERIFY_IS_FALSE(path.Pieces.empty());

            // It starts and ends exactly on the two points.
            VERIFY_ARE_EQUAL(request.From.X, path.Start.X);
            VERIFY_ARE_EQUAL(request.From.Y, path.Start.Y);
            VERIFY_IS_TRUE(std::abs(path.Pieces.back().End.X - request.To.X) < 1e-6);
            VERIFY_IS_TRUE(std::abs(path.Pieces.back().End.Y - request.To.Y) < 1e-6);

            // Out to the right, and in from the left.
            auto const leaving = Leaving(path.Start, path.Pieces.front());
            VERIFY_IS_TRUE(leaving.X > 0.9999);

            auto lastStart = path.Start;

            for (size_t piece = 0; piece + 1 < path.Pieces.size(); piece++)
            {
                lastStart = path.Pieces[piece].End;
            }

            auto const arriving = Arriving(lastStart, path.Pieces.back());
            VERIFY_IS_TRUE(arriving.X > 0.9999);

            // No corners: each piece sets off the way the one before it arrived.
            auto start = path.Start;
            auto sharpest = 0.0;

            for (size_t piece = 0; piece + 1 < path.Pieces.size(); piece++)
            {
                auto const arrived = Arriving(start, path.Pieces[piece]);
                auto const next = Leaving(path.Pieces[piece].End, path.Pieces[piece + 1]);

                sharpest = (std::max)(sharpest, DegreesBetween(arrived, next));

                start = path.Pieces[piece].End;
            }

            if (sharpest >= 0.5)
            {
                Log::Comment(String().Format(L"Layout %u, line %zu turns %.1f degrees between two pieces", seed, i, sharpest));
            }

            VERIFY_IS_TRUE(sharpest < 0.5);
        }
    }
}

void CordPathTests::LinesSharingAWayAreSpreadApart()
{
    // Two lines from one node back to another level with it. Both go under the two nodes, the same
    // way, so they would be drawn on top of each other.
    Layout layout{};
    layout.Nodes = { CordRect{ 48, 100, 328, 260 }, CordRect{ 700, 100, 980, 260 } };
    layout.Links = { Link{ 1, 182, 0, 182 }, Link{ 1, 214, 0, 214 } };

    auto const paths = Plan(layout);

    VERIFY_ARE_EQUAL(size_t{ 2 }, paths.size());

    auto const first = LongestLevelRun(paths[0]);
    auto const second = LongestLevelRun(paths[1]);

    VERIFY_IS_TRUE(first.has_value());
    VERIFY_IS_TRUE(second.has_value());

    // They share the stretch under both nodes...
    VERIFY_IS_TRUE((std::min)(first->second.second, second->second.second) - (std::max)(first->second.first, second->second.first) > 300);

    // ...ten pixels apart rather than on top of each other.
    Log::Comment(String().Format(L"Level runs at %.1f and %.1f", first->first, second->first));
    VERIFY_IS_TRUE(std::abs(first->first - second->first) >= 9.99);

    for (auto const& path : paths)
    {
        VERIFY_ARE_EQUAL(size_t{ 0 }, NodesCrossed(path, layout.Nodes));
    }
}

void CordPathTests::LabelsSitOnTheirLinesClearOfEachOther()
{
    Layout layout{};
    layout.Nodes = { CordRect{ 48, 100, 328, 260 }, CordRect{ 700, 100, 980, 260 } };
    layout.Links = { Link{ 1, 182, 0, 182 }, Link{ 1, 214, 0, 214 }, Link{ 1, 246, 0, 246 } };

    auto const paths = Plan(layout);

    VERIFY_ARE_EQUAL(size_t{ 3 }, paths.size());

    for (size_t i = 0; i < paths.size(); i++)
    {
        VERIFY_IS_TRUE(DistanceToLine(paths[i].LabelPoint, paths[i]) < 0.75);

        for (size_t j = i + 1; j < paths.size(); j++)
        {
            auto const& a = paths[i].LabelPoint;
            auto const& b = paths[j].LabelPoint;

            // A label is about 64 by 22.
            VERIFY_IS_TRUE(std::abs(a.X - b.X) >= 64 || std::abs(a.Y - b.Y) >= 22);
        }
    }
}

void CordPathTests::TheSameLayoutGivesTheSameLines()
{
    for (unsigned seed = 500; seed < 520; seed++)
    {
        auto const layout = RandomLayout(seed);
        auto const first = Plan(layout);
        auto const second = Plan(layout);

        VERIFY_ARE_EQUAL(first.size(), second.size());

        auto same = true;

        for (size_t i = 0; i < first.size(); i++)
        {
            same = same && first[i].Pieces.size() == second[i].Pieces.size() &&
                first[i].LabelPoint.X == second[i].LabelPoint.X &&
                first[i].LabelPoint.Y == second[i].LabelPoint.Y;

            for (size_t piece = 0; same && piece < first[i].Pieces.size(); piece++)
            {
                auto const& a = first[i].Pieces[piece];
                auto const& b = second[i].Pieces[piece];

                same = a.End.X == b.End.X && a.End.Y == b.End.Y &&
                    a.Control1.X == b.Control1.X && a.Control1.Y == b.Control1.Y &&
                    a.Control2.X == b.Control2.X && a.Control2.Y == b.Control2.Y &&
                    a.IsStraight == b.IsStraight;
            }
        }

        VERIFY_IS_TRUE(same);
    }
}

void CordPathTests::ABigPatchIsQuick()
{
    // The canvas plans every line again each time a node moves under the pointer, so it has to
    // be quick: 112 nodes in 8 columns, each feeding the next column, and a third looping back.
    Layout layout{};

    for (int column = 0; column < 8; column++)
    {
        for (int row = 0; row < 14; row++)
        {
            auto const x = 48.0 + column * 360.0;
            auto const y = 48.0 + row * 131.0;

            layout.Nodes.push_back(CordRect{ x, y, x + 280, y + 99 });
        }
    }

    for (size_t column = 0; column + 1 < 8; column++)
    {
        for (size_t row = 0; row < 14; row++)
        {
            auto const from = column * 14 + row;
            auto const to = (column + 1) * 14 + (row * 5) % 14;

            layout.Links.push_back(Link{ from, layout.Nodes[from].Top + 60, to, layout.Nodes[to].Top + 60 });

            if (row % 3 == 0)
            {
                auto const back = ((column + 7) % 8) * 14 + (row + 4) % 14;
                layout.Links.push_back(Link{ from, layout.Nodes[from].Top + 60, back, layout.Nodes[back].Top + 60 });
            }
        }
    }

    auto fastest = std::chrono::microseconds::max();
    std::vector<CordPath> paths{};

    for (int run = 0; run < 3; run++)
    {
        auto const started = std::chrono::steady_clock::now();
        paths = Plan(layout);
        fastest = (std::min)(fastest, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started));
    }

    auto const around = std::count_if(paths.begin(), paths.end(), [](CordPath const& p) { return p.GoesAround; });

    Log::Comment(String().Format(L"%zu nodes, %zu lines, %zd went around something: %lld microseconds",
        layout.Nodes.size(), layout.Links.size(), around, static_cast<long long>(fastest.count())));

    VERIFY_ARE_EQUAL(layout.Links.size(), paths.size());
    VERIFY_IS_TRUE(around > 0);
    VERIFY_IS_TRUE(fastest < std::chrono::milliseconds{ 500 });

    auto const crossing = std::count_if(paths.begin(), paths.end(),
        [&layout](CordPath const& path) { return NodesCrossed(path, layout.Nodes) != 0; });

    VERIFY_ARE_EQUAL(std::ptrdiff_t{ 0 }, crossing);
}

void CordPathTests::NonsenseDoesNotStopIt()
{
    auto const nan = std::numeric_limits<double>::quiet_NaN();
    auto const infinity = std::numeric_limits<double>::infinity();

    std::vector<CordRect> const nodes{
        CordRect{ 0, 0, 200, 100 },
        CordRect{ nan, 0, 100, 100 },               // not a number
        CordRect{ 300, 300, 200, 200 },             // inside out
        CordRect{ 0, 0, infinity, infinity },       // endless
        CordRect{ 400, 0, 600, 100 },
    };

    // An end that isn't a number draws nothing.
    auto const nothing = PlanCordPath(CordPoint{ nan, 50 }, CordPoint{ 400, 50 }, nodes, 0, 4);
    VERIFY_IS_TRUE(nothing.Pieces.empty());

    // The broken nodes are left out, and the rest still get a line.
    for (auto const& [from, to] : { std::pair{ CordPoint{ 200, 50 }, CordPoint{ 400, 50 } },
                                    std::pair{ CordPoint{ 600, 50 }, CordPoint{ 0, 50 } },
                                    std::pair{ CordPoint{ 200, 50 }, CordPoint{ 200, 50 } },
                                    std::pair{ CordPoint{ 150, 50 }, CordPoint{ 450, 50 } } })
    {
        auto const path = PlanCordPath(from, to, nodes, 0, 4);

        VERIFY_IS_FALSE(path.Pieces.empty());
        VERIFY_IS_TRUE(std::abs(path.Pieces.back().End.X - to.X) < 1e-6);
        VERIFY_IS_TRUE(std::abs(path.Pieces.back().End.Y - to.Y) < 1e-6);

        auto const samples = SampleCordPath(path, 8);

        VERIFY_IS_TRUE(std::all_of(samples.begin(), samples.end(),
            [](CordPoint const& p) { return std::isfinite(p.X) && std::isfinite(p.Y); }));
    }

    // No nodes at all, and nothing to plan.
    VERIFY_IS_FALSE(PlanCordPath(CordPoint{ 500, 50 }, CordPoint{ 100, 80 }, {}, NoNode, NoNode).Pieces.empty());
    VERIFY_IS_TRUE(PlanCordPaths({}, nodes).empty());
}
