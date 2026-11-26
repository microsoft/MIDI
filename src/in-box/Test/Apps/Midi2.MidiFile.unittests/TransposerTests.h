// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The transposer every note of a playing sequence goes through. A mistake here is either a wrong
// note or a note that never ends, so these check the words that go out, one by one.
class TransposerTests : public WEX::TestClass<TransposerTests>
{
public:

    BEGIN_TEST_CLASS(TransposerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ChangesNothingAtZero);
    TEST_METHOD(ClampsToTheLimits);
    TEST_METHOD(MovesNotesAndTheirEnds);
    TEST_METHOD(LeavesDrumsAlone);
    TEST_METHOD(KeepsASoundingNoteAtItsPitch);
    TEST_METHOD(EndsAKeyStruckAgainAtANewPitch);
    TEST_METHOD(SendsEveryEndOfARestruckKeyToItsPitch);
    TEST_METHOD(DropsNotesOutOfRange);
    TEST_METHOD(KeepsASoundingNoteWhenItsRestartIsOutOfRange);
    TEST_METHOD(TreatsVelocityZeroAsAnEnd);
    TEST_METHOD(MovesMidi2NotesAndPerNoteMessages);
    TEST_METHOD(MovesMidi2PitchAttributes);
    TEST_METHOD(CopiesOtherPacketsUnchanged);
    TEST_METHOD(ForgetsEverythingWhenCleared);
    TEST_METHOD(StartsNotesForTheEngine);
};
