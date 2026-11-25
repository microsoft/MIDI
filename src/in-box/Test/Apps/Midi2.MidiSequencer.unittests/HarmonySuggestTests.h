// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Help picking chords on the Key and chords row: what could come next, more chords, progressions,
// Fill and Suggest.
class HarmonySuggestTests : public WEX::TestClass<HarmonySuggestTests>
{
public:

    BEGIN_TEST_CLASS(HarmonySuggestTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NextChordsInAMajorKey);
    TEST_METHOD(NextChordsInAMinorKey);
    TEST_METHOD(StartingChords);
    TEST_METHOD(PhraseEndsPreferHomeAndPullingBack);
    TEST_METHOD(SecondaryDominantsResolve);
    TEST_METHOD(SuggestionsAreDeterministicAndNeverRepeatTheChord);
    TEST_METHOD(MoreChordsAreBorrowed);
    TEST_METHOD(ProgressionsInMajorAndMinor);
    TEST_METHOD(ProgressionLayoutFollowsTheRate);
    TEST_METHOD(ProgressionLayoutFromMidBar);
    TEST_METHOD(SuggestFillsUpToTheNextChord);
    TEST_METHOD(SuggestFillsToThePhraseEnd);
    TEST_METHOD(SuggestNeedsAKey);
};
