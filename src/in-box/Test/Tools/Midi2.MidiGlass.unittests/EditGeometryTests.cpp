// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "EditGeometryTests.h"

#include "EditGeometry.h"

#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    constexpr double Tolerance = 0.0001;

    void VerifyNear(_In_ double expected, _In_ double actual)
    {
        VERIFY_IS_LESS_THAN(std::abs(expected - actual), Tolerance);
    }

    glass::SnapSettings DefaultSettings() noexcept
    {
        glass::SnapSettings settings{};

        settings.GridEnabled = true;
        settings.GridSize = 8.0;
        settings.GuidesEnabled = true;
        settings.Threshold = 6.0;

        return settings;
    }

    glass::EditRect const Page{ 0.0, 0.0, 1280.0, 800.0 };
}

// ---- the grid ----

void EditGeometryTests::SnapRoundsToTheNearestCell()
{
    VerifyNear(8.0, glass::SnapToGrid(9.0, 8.0));
    VerifyNear(16.0, glass::SnapToGrid(13.0, 8.0));
    VerifyNear(0.0, glass::SnapToGrid(3.0, 8.0));
    VerifyNear(-8.0, glass::SnapToGrid(-9.0, 8.0));
}

void EditGeometryTests::SnapWithNoGridLeavesTheValueAlone()
{
    VerifyNear(9.0, glass::SnapToGrid(9.0, 0.0));
    VerifyNear(9.0, glass::SnapToGrid(9.0, -8.0));
}

// ---- magnetic guides ----

void EditGeometryTests::AGuideBeatsTheGrid()
{
    // Somebody lining a control up with the one beside it means that edge, not the nearest
    // multiple of eight. A drawing app that did it the other way round would feel broken.
    std::vector<glass::EditRect> const others{ { 101.0, 300.0, 40.0, 180.0 } };

    glass::EditRect const moving{ 99.0, 500.0, 40.0, 180.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(101.0, outcome.X);
    VERIFY_ARE_EQUAL(size_t{ 1 }, outcome.Guides.size());
}

void EditGeometryTests::LeftEdgesLineUp()
{
    std::vector<glass::EditRect> const others{ { 200.0, 100.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 203.0, 400.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(200.0, outcome.X);
}

void EditGeometryTests::CentersLineUp()
{
    // The neighbor's center is at 228. The moving control is 100 wide, so its own center lands
    // there when its left edge is at 178.
    std::vector<glass::EditRect> const others{ { 200.0, 100.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 180.0, 400.0, 100.0, 40.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(178.0, outcome.X);
}

void EditGeometryTests::APageMarginPulls()
{
    std::vector<glass::EditRect> const others{};

    glass::EditRect const moving{ 3.0, 400.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(0.0, outcome.X);
}

void EditGeometryTests::SomethingFarAwayDoesNotPull()
{
    std::vector<glass::EditRect> const others{ { 600.0, 100.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 203.0, 300.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    // Nothing in range, so the grid has it.
    VerifyNear(200.0, outcome.X);
    VERIFY_ARE_EQUAL(size_t{ 0 }, outcome.Guides.size());
}

void EditGeometryTests::TheNearestCandidateWins()
{
    std::vector<glass::EditRect> const others
    {
        { 205.0, 100.0, 56.0, 56.0 },
        { 201.0, 600.0, 56.0, 56.0 },
    };

    glass::EditRect const moving{ 202.0, 400.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(201.0, outcome.X);
}

void EditGeometryTests::AGuideReachesBothRectangles()
{
    std::vector<glass::EditRect> const others{ { 200.0, 100.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 203.0, 300.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VERIFY_ARE_EQUAL(size_t{ 1 }, outcome.Guides.size());

    auto const& guide = outcome.Guides[0];

    VERIFY_IS_TRUE(guide.Axis == glass::GuideAxis::Vertical);
    VerifyNear(100.0, guide.Start);
    VerifyNear(356.0, guide.End);
}

void EditGeometryTests::SuspendingSnapLeavesThePositionExact()
{
    // Alt suspends snapping while a drag is under way, so free-form placement is always
    // available without changing a setting.
    auto settings = DefaultSettings();
    settings.GridEnabled = false;
    settings.GuidesEnabled = false;

    std::vector<glass::EditRect> const others{ { 200.0, 100.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 203.0, 401.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, settings);

    VerifyNear(203.0, outcome.X);
    VerifyNear(401.0, outcome.Y);
    VERIFY_ARE_EQUAL(size_t{ 0 }, outcome.Guides.size());
}

void EditGeometryTests::BothAxesCanSnapAtOnce()
{
    std::vector<glass::EditRect> const others{ { 200.0, 300.0, 56.0, 56.0 } };

    glass::EditRect const moving{ 202.0, 302.0, 56.0, 56.0 };

    auto const outcome = glass::SnapMove(moving, others, Page, DefaultSettings());

    VerifyNear(200.0, outcome.X);
    VerifyNear(300.0, outcome.Y);
    VERIFY_ARE_EQUAL(size_t{ 2 }, outcome.Guides.size());
}

// ---- handles ----

void EditGeometryTests::DraggingTheRightEdgeLeavesTheLeftAlone()
{
    glass::EditRect const start{ 100.0, 100.0, 56.0, 56.0 };

    auto const resized = glass::ApplyResize(start, glass::ResizeHandle::Right, 20.0, 999.0, false);

    VerifyNear(100.0, resized.X);
    VerifyNear(76.0, resized.Width);
    VerifyNear(56.0, resized.Height);
}

void EditGeometryTests::DraggingTheLeftEdgeMovesTheOrigin()
{
    glass::EditRect const start{ 100.0, 100.0, 56.0, 56.0 };

    auto const resized = glass::ApplyResize(start, glass::ResizeHandle::Left, -20.0, 0.0, false);

    VerifyNear(80.0, resized.X);
    VerifyNear(76.0, resized.Width);
}

void EditGeometryTests::AControlCannotBeDraggedToNothing()
{
    glass::EditRect const start{ 100.0, 100.0, 56.0, 56.0 };

    auto const resized = glass::ApplyResize(start, glass::ResizeHandle::Right, -500.0, 0.0, false);

    VerifyNear(glass::MinimumControlSize, resized.Width);
    VERIFY_IS_GREATER_THAN(resized.Width, 0.0);
}

void EditGeometryTests::ACornerCanHoldTheAspectRatio()
{
    glass::EditRect const start{ 100.0, 100.0, 40.0, 80.0 };

    auto const resized = glass::ApplyResize(start, glass::ResizeHandle::BottomRight, 40.0, 0.0, true);

    VerifyNear(80.0, resized.Width);
    VerifyNear(160.0, resized.Height);
}

void EditGeometryTests::AnEdgeHandleCannotHoldTheAspectRatio()
{
    // Only one dimension is being dragged, so there is nothing to hold the other against.
    glass::EditRect const start{ 100.0, 100.0, 40.0, 80.0 };

    auto const resized = glass::ApplyResize(start, glass::ResizeHandle::Right, 40.0, 0.0, true);

    VerifyNear(80.0, resized.Width);
    VerifyNear(80.0, resized.Height);
}

void EditGeometryTests::ResizingSnapsOnlyTheEdgeBeingDragged()
{
    std::vector<glass::EditRect> const others{ { 300.0, 100.0, 56.0, 56.0 } };

    // Right edge is at 297, three from the neighbor's left edge.
    glass::EditRect const moving{ 100.0, 400.0, 197.0, 56.0 };

    auto const outcome = glass::SnapResize(
        moving, glass::ResizeHandle::Right, others, Page, DefaultSettings());

    VerifyNear(300.0, outcome.X);
}

// ---- off the page ----

void EditGeometryTests::AControlHalfOffThePageCountsAsOutside()
{
    // A control half off the page is just as unreachable at run time as one entirely off it.
    VERIFY_IS_TRUE(glass::IsOutsidePage({ 1250.0, 100.0, 56.0, 56.0 }, 1280.0, 800.0));
    VERIFY_IS_TRUE(glass::IsOutsidePage({ -4.0, 100.0, 56.0, 56.0 }, 1280.0, 800.0));
    VERIFY_IS_TRUE(glass::IsOutsidePage({ 100.0, 780.0, 56.0, 56.0 }, 1280.0, 800.0));
}

void EditGeometryTests::AControlOnTheEdgeIsInside()
{
    VERIFY_IS_FALSE(glass::IsOutsidePage({ 0.0, 0.0, 56.0, 56.0 }, 1280.0, 800.0));
    VERIFY_IS_FALSE(glass::IsOutsidePage({ 1224.0, 744.0, 56.0, 56.0 }, 1280.0, 800.0));
}

// ---- changing the page size ----

void EditGeometryTests::GrowingWithATopLeftAnchorMovesNothing()
{
    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 1920.0, 1080.0, glass::CanvasAnchor::TopLeft, false);

    VerifyNear(1.0, transform.Scale);
    VerifyNear(0.0, transform.OffsetX);
    VerifyNear(0.0, transform.OffsetY);
}

void EditGeometryTests::GrowingWithACenterAnchorCentersEverything()
{
    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 1920.0, 1080.0, glass::CanvasAnchor::Center, false);

    VerifyNear(1.0, transform.Scale);
    VerifyNear(320.0, transform.OffsetX);
    VerifyNear(140.0, transform.OffsetY);
}

void EditGeometryTests::GrowingWithABottomRightAnchorTakesAllTheSpace()
{
    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 1920.0, 1080.0, glass::CanvasAnchor::BottomRight, false);

    VerifyNear(640.0, transform.OffsetX);
    VerifyNear(280.0, transform.OffsetY);
}

void EditGeometryTests::ScalingToFitUsesOneFactorForBothAxes()
{
    // Scaling the axes independently would stretch a knob into an ellipse, which is the same
    // reason a page is letterboxed at run time rather than stretched.
    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 640.0, 600.0, glass::CanvasAnchor::TopLeft, true);

    VerifyNear(0.5, transform.Scale);
}

void EditGeometryTests::ScalingToFitCentersWhatIsLeftOver()
{
    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 640.0, 600.0, glass::CanvasAnchor::TopLeft, true);

    VerifyNear(0.0, transform.OffsetX);
    VerifyNear(100.0, transform.OffsetY);
}

void EditGeometryTests::ShrinkingAndLeavingCountsWhatFallsOutside()
{
    std::vector<glass::EditRect> const rects
    {
        { 0.0, 0.0, 56.0, 56.0 },
        { 900.0, 100.0, 56.0, 56.0 },
        { 1100.0, 100.0, 56.0, 56.0 },
    };

    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 1024.0, 768.0, glass::CanvasAnchor::TopLeft, false);

    VERIFY_ARE_EQUAL(size_t{ 1 }, glass::CountOutsidePage(rects, transform, 1024.0, 768.0));
}

void EditGeometryTests::ScalingToFitLeavesNothingOutside()
{
    std::vector<glass::EditRect> const rects
    {
        { 0.0, 0.0, 56.0, 56.0 },
        { 900.0, 100.0, 56.0, 56.0 },
        { 1100.0, 700.0, 56.0, 56.0 },
    };

    auto const transform = glass::ComputePageResize(
        1280.0, 800.0, 1024.0, 768.0, glass::CanvasAnchor::TopLeft, true);

    VERIFY_ARE_EQUAL(size_t{ 0 }, glass::CountOutsidePage(rects, transform, 1024.0, 768.0));
}

void EditGeometryTests::GrowThenShrinkBackPutsEverythingWhereItStarted()
{
    // The exit criterion for the virtual canvas: a page grown and then shrunk back leaves every
    // control exactly where it was. That only holds because nothing is ever clamped.
    glass::EditRect const start{ 1100.0, 700.0, 56.0, 56.0 };

    auto const grow = glass::ComputePageResize(
        1280.0, 800.0, 1920.0, 1080.0, glass::CanvasAnchor::Center, false);

    auto const grown = glass::ApplyTransform(start, grow);

    auto const shrink = glass::ComputePageResize(
        1920.0, 1080.0, 1280.0, 800.0, glass::CanvasAnchor::Center, false);

    auto const back = glass::ApplyTransform(grown, shrink);

    VerifyNear(start.X, back.X);
    VerifyNear(start.Y, back.Y);
    VerifyNear(start.Width, back.Width);
    VerifyNear(start.Height, back.Height);
}

// ---- the work area ----

void EditGeometryTests::TheWorkAreaSurroundsThePage()
{
    auto const area = glass::ComputeWorkArea(1280.0, 800.0, {});

    VERIFY_IS_LESS_THAN(area.X, 0.0);
    VERIFY_IS_LESS_THAN(area.Y, 0.0);
    VERIFY_IS_GREATER_THAN(area.Right(), 1280.0);
    VERIFY_IS_GREATER_THAN(area.Bottom(), 800.0);
}

void EditGeometryTests::TheWorkAreaGrowsToHoldAnOffPageControl()
{
    // A control parked outside the page has to stay reachable. Clamping it inside instead is
    // what would make a page resize unrecoverable.
    std::vector<glass::EditRect> const rects{ { -600.0, 100.0, 56.0, 56.0 } };

    auto const area = glass::ComputeWorkArea(1280.0, 800.0, rects);

    VERIFY_IS_LESS_THAN(area.X, -600.0);
}
