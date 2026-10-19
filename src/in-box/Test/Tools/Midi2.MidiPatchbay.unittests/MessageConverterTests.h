// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The message converter: what each row turns a message into, pressing and letting go, and the
// file format that carries it.
class MessageConverterTests : public WEX::TestClass<MessageConverterTests>
{
public:

    BEGIN_TEST_CLASS(MessageConverterTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ANoteBecomesAControlChange);
    TEST_METHOD(LettingGoCanSendNothingOrToggle);
    TEST_METHOD(AControlChangeCanBeAButton);
    TEST_METHOD(AMovingValueIsConvertedAsItComes);
    TEST_METHOD(TheFormatCanStayOrChange);
    TEST_METHOD(ANoteFromSomethingThatNeverLetsGoEndsAtOnce);
    TEST_METHOD(ProgramChangesStepThroughTheirRange);
    TEST_METHOD(TheFirstRowThatMatchesIsUsed);
    TEST_METHOD(AMessageKeepsItsGroup);
    TEST_METHOD(AConverterRunsInARoutedPatch);

    // ---- the file ----
    TEST_METHOD(ConverterSettingsSurviveTheFile);
    TEST_METHOD(BadValuesInTheFileFallBack);
    TEST_METHOD(AFileWrittenTheWayTheGuideSaysReads);
};
