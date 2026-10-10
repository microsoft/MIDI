// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The sequence model: tempo and meter maths, value scaling, chance, and the clean-up a sequence
// gets after it's read.
class SequenceModelTests : public WEX::TestClass<SequenceModelTests>
{
public:

    BEGIN_TEST_CLASS(SequenceModelTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ASteadyTempoIsLinear);
    TEST_METHOD(ARampFollowsTheCurve);
    TEST_METHOD(BarsFollowMeterChanges);
    TEST_METHOD(ScalingUpThenDownGivesBackTheOriginal);
    TEST_METHOD(ChanceIsTheSameEveryTime);
    TEST_METHOD(NormalizingFixesWhatItCan);
    TEST_METHOD(ATrackIsFoundInsideFolders);
    TEST_METHOD(TextSurvivesUtf8);
};

class SequenceSerializerTests : public WEX::TestClass<SequenceSerializerTests>
{
public:

    BEGIN_TEST_CLASS(SequenceSerializerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ASequenceRoundTrips);
    TEST_METHOD(KeysFromANewerBuildSurvive);
    TEST_METHOD(ANewerFileIsFlagged);
    TEST_METHOD(SomethingElseIsRefused);
    TEST_METHOD(DamagedValuesAreClampedOrSkipped);
    TEST_METHOD(CountsStopAtTheLimits);
    TEST_METHOD(AnotherResolutionIsScaled);
    TEST_METHOD(MessagesReadAndWriteAsText);
};

class SequenceRenderTests : public WEX::TestClass<SequenceRenderTests>
{
public:

    BEGIN_TEST_CLASS(SequenceRenderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ALinkedClipPlaysAtEveryPlacement);
    TEST_METHOD(ALoopRepeatsAndCutsNotesAtTheLoop);
    TEST_METHOD(NoteEndsComeBeforeNoteStartsAtOneTick);
    TEST_METHOD(AWindowHoldsWhatStartsInIt);
    TEST_METHOD(TheDestinationSetsGroupAndChannel);
    TEST_METHOD(AHostileLoopStopsAtTheCeiling);
};

class SequenceUndoTests : public WEX::TestClass<SequenceUndoTests>
{
public:

    BEGIN_TEST_CLASS(SequenceUndoTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NoteEditsUndoAndRedo);
    TEST_METHOD(ASnapshotUndoesAndRedoes);
    TEST_METHOD(ANewEditForgetsTheRedo);
    TEST_METHOD(TheHistoryStaysUnderItsCeiling);
};
