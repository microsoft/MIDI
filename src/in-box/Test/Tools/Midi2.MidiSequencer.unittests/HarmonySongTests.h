// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Ask an AI assistant on the Key and chords row: reading the song plan an assistant writes back,
// the checks that refuse it, and laying it out on the row and the tags.
class HarmonySongTests : public WEX::TestClass<HarmonySongTests>
{
public:

    BEGIN_TEST_CLASS(HarmonySongTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ReadsTheGuidesExample);
    TEST_METHOD(FindsTheJsonInAnAnswer);
    TEST_METHOD(RefusesWithAReason);
    TEST_METHOD(BeatsMustFillTheBars);
    TEST_METHOD(LimitsAreChecked);
    TEST_METHOD(LaysOutKeysChordsAndTags);
    TEST_METHOD(SectionColors);
    TEST_METHOD(NullMeansLeftOut);
    TEST_METHOD(AddingClearsTheSongsSpan);
    TEST_METHOD(MeterChangesInsideTheSong);
};
