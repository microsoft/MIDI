// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The recent sequences list and its pinned favorites (design section 26).
class RecentSequencesTests : public WEX::TestClass<RecentSequencesTests>
{
public:

    BEGIN_TEST_CLASS(RecentSequencesTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TheNewestIsFirstAndOpeningAgainMovesItUp);
    TEST_METHOD(PathsMatchWithoutCaseOrSlashDirection);
    TEST_METHOD(TheOldestAgeOutPastTheLimit);
    TEST_METHOD(PinnedSequencesNeverAgeOut);
    TEST_METHOD(OpeningAgainKeepsThePin);
    TEST_METHOD(PinningHasALimitAndNeedsAnEntry);
    TEST_METHOD(ForgettingRemovesEverySpelling);
    TEST_METHOD(PinnedComeFirstWhenShown);
    TEST_METHOD(TheListRoundTrips);
    TEST_METHOD(ReadingSkipsWhatIsntASequence);
    TEST_METHOD(ReadingMergesTheSameSequence);
};
