// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The timeline's view and the arithmetic its scroll bar does on it (design section 31).
class TimelineViewTests : public WEX::TestClass<TimelineViewTests>
{
public:

    BEGIN_TEST_CLASS(TimelineViewTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TheViewStaysInsideItsLimits);
    TEST_METHOD(ZoomKeepsTheAnchorWhereItWas);
    TEST_METHOD(ZoomStopsAtItsLimits);
    TEST_METHOD(DraggingAnEndKeepsTheOtherEnd);
    TEST_METHOD(FittingShowsTheWholeRange);
    TEST_METHOD(TheExtentHasRoomAndTheView);
    TEST_METHOD(TheBarsScaleRoundTrips);
    TEST_METHOD(NonsenseLeavesASensibleView);
};
