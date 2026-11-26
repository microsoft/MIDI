// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Generator clips: Euclidean rhythms, chance melodies, the file, Make notes and undo.
class GeneratorTests : public WEX::TestClass<GeneratorTests>
{
public:

    BEGIN_TEST_CLASS(GeneratorTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(EuclideanRhythmsSpreadEvenly);
    TEST_METHOD(RingsRunTheirOwnLength);
    TEST_METHOD(RingChanceRepeatsUnlessItChanges);
    TEST_METHOD(AChanceMelodyStaysInItsScaleAndRange);
    TEST_METHOD(KeepFromTheLastPass);
    TEST_METHOD(AnArpeggiatorPlaysNothingYet);
    TEST_METHOD(APieceAtATimeMatchesTheWhole);
    TEST_METHOD(GeneratorsRoundTripThroughTheFile);
    TEST_METHOD(DamagedGeneratorsAreClamped);
    TEST_METHOD(MakeNotesPlaysWhatTheGeneratorPlayed);
    TEST_METHOD(GeneratorEditsUndo);
};
