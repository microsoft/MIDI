// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Starter patterns: every template is well formed and plays, a melodic template follows the key,
// held notes, accents and slides read right, and a template goes on top of a pattern's own steps
// without losing them.
class PatternTemplatesTests : public WEX::TestClass<PatternTemplatesTests>
{
public:

    BEGIN_TEST_CLASS(PatternTemplatesTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(EveryTemplateIsWellFormed);
    TEST_METHOD(ADrumTemplateMakesItsLanes);
    TEST_METHOD(ADrumTemplatePlays);
    TEST_METHOD(AMelodicTemplateFollowsTheKey);
    TEST_METHOD(HeldNotesAccentsAndSlides);
    TEST_METHOD(OverlayKeepsWhatWasThere);
    TEST_METHOD(OverlayKeepsTimeAcrossStepSizes);
    TEST_METHOD(OverlayOnAMelodicPattern);
};
