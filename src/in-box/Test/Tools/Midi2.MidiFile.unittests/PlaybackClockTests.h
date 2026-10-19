// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The clock behind the player's speed control. It is plain arithmetic, so it is tested here on a
// made-up clock rather than through the service, where timing would make the results unreliable.
class PlaybackClockTests : public WEX::TestClass<PlaybackClockTests>
{
public:

    BEGIN_TEST_CLASS(PlaybackClockTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the written speed ----
    TEST_METHOD(MatchesTheOriginalArithmeticAtTheWrittenSpeed);
    TEST_METHOD(HoldsThePositionDuringTheStartLead);

    // ---- other speeds ----
    TEST_METHOD(ScalesTimeByTheRate);
    TEST_METHOD(ClampsTheRateAndIgnoresValuesThatAreNotNumbers);

    // ---- changing speed while playing ----
    TEST_METHOD(StartsANewRateAtTheEndOfWhatWasHandedOver);
    TEST_METHOD(HoldsASecondChangeUntilTheFirstHasStarted);
    TEST_METHOD(AppliesAWaitingChangeAtTheNextStart);
    TEST_METHOD(KeepsEveryMessageInOrderWhileTheRateMoves);
};
