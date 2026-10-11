// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Pattern clips: what the steps play, lanes with their own length, swing, nudges, ratchets,
// conditions and chance, the file, the drawn notes, Make notes and undo.
class PatternTests : public WEX::TestClass<PatternTests>
{
public:

    BEGIN_TEST_CLASS(PatternTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(APatternPlaysItsSteps);
    TEST_METHOD(LanesKeepTheirOwnLength);
    TEST_METHOD(SwingAndNudgeMoveSteps);
    TEST_METHOD(RatchetsRepeatInsideTheStep);
    TEST_METHOD(ConditionsCountPassesThroughTheLane);
    TEST_METHOD(ChanceIsTheSameEveryTime);
    TEST_METHOD(MelodicStepsPlayTheirOwnNotes);
    TEST_METHOD(MutedAndSoloedLanes);
    TEST_METHOD(APieceAtATimeMatchesTheWhole);
    TEST_METHOD(PatternsRoundTripThroughTheFile);
    TEST_METHOD(DamagedPatternsAreClampedOrSkipped);
    TEST_METHOD(ThePreviewFollowsThePattern);
    TEST_METHOD(MakeNotesPlaysWhatThePatternPlays);
    TEST_METHOD(PatternEditsUndo);
};
