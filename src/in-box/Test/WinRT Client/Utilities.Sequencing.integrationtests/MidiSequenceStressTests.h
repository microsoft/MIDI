// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSequenceStressTests
    : public WEX::TestClass<MidiSequenceStressTests>
{
    BEGIN_TEST_CLASS(MidiSequenceStressTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Stress")
        TEST_CLASS_PROPERTY(L"Description", L"Trying to break the sequencing API")
    END_TEST_CLASS()

    // ---- dense content ----
    TEST_METHOD(ReadsDenseContentQuickly);
    TEST_METHOD(WindowedReadsStayCheapOnDenseContent);
    TEST_METHOD(PlaysDenseContentWithoutFalteringOrHanging);

    // ---- hammering the transport ----
    TEST_METHOD(SurvivesRapidStartAndStop);
    TEST_METHOD(SurvivesSeekingWhilePlaying);
    TEST_METHOD(SurvivesMuteAndSoloChurnWhilePlaying);
    TEST_METHOD(SurvivesSequenceSwapsWhilePlaying);
    TEST_METHOD(SurvivesClosingWhilePlaying);
    TEST_METHOD(SurvivesManyPlayersInSuccession);

    // ---- fuzzing the reader ----
    TEST_METHOD(SurvivesEveryTruncationOfAFile);
    TEST_METHOD(SurvivesSingleByteCorruption);
    TEST_METHOD(SurvivesRandomBytes);
    TEST_METHOD(SurvivesAFileOfOnlyHeaders);
    TEST_METHOD(SurvivesDeeplyNestedVariableLengthQuantities);
};
