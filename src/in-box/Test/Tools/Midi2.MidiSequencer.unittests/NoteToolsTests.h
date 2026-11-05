// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The piano roll's note tools (design section 34): each one changes only what it should, is the
// same every time for the same settings, and keeps notes in range.
class NoteToolsTests : public WEX::TestClass<NoteToolsTests>
{
public:

    BEGIN_TEST_CLASS(NoteToolsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(CurvesStartAtZeroAndEndAtOne);
    TEST_METHOD(ChordsAreFoundByStartTime);
    TEST_METHOD(QuantizeWithStrengthSwingAndKeep);
    TEST_METHOD(QuantizeEnds);
    TEST_METHOD(HumanizeIsRepeatableAndTighterOnTheBeat);
    TEST_METHOD(GrooveFromAnotherClip);
    TEST_METHOD(TimeFeelKeepsTheFirstStartAndLastEnd);
    TEST_METHOD(RatchetRepeatsInsideTheNote);
    TEST_METHOD(LegatoFixedAndPercentLengths);
    TEST_METHOD(CleanUpFixesOverlapsDuplicatesAndShortNotes);
    TEST_METHOD(TransposeInKeyStaysInTheKey);
    TEST_METHOD(FitToChordsUsesTheChordThenTheKey);
    TEST_METHOD(VoicingInversionsDropsAndOctaves);
    TEST_METHOD(SmoothVoiceLeadingMovesLittle);
    TEST_METHOD(MirrorAndReverse);
    TEST_METHOD(ValuesSetRampAccentAndChance);
    TEST_METHOD(SelectingTopBottomEveryNthAndInvert);
};
