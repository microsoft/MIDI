// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ThemeFileTests : public WEX::TestClass<ThemeFileTests>
{
public:

    BEGIN_TEST_CLASS(ThemeFileTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(WritesColorsThePeopleCanRead);
    TEST_METHOD(ParsesBothColorForms);
    TEST_METHOD(RejectsColorsThatAreNotColors);
    TEST_METHOD(EveryShippedThemeSurvivesARoundTrip);
    TEST_METHOD(WritingTheSameThemeTwiceProducesTheSameBytes);
    TEST_METHOD(ReadsAHandAuthoredTheme);
    TEST_METHOD(AHalfWrittenThemeIsStillUsable);
    TEST_METHOD(AThemeFileCannotClaimToBeBuiltIn);
    TEST_METHOD(RefusesADeckImageThatEscapesItsFolder);
    TEST_METHOD(SurvivesAHostileThemeFile);
};
