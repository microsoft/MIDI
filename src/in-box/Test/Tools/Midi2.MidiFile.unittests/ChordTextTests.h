// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The chord symbols the player shows, moved into the key the music is transposed to.
class ChordTextTests : public WEX::TestClass<ChordTextTests>
{
public:

    BEGIN_TEST_CLASS(ChordTextTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(FindsTheKeyTheMusicMovesTo);
    TEST_METHOD(LeavesChordsAloneWhenTheKeyDoesNotChange);
    TEST_METHOD(MovesTheRootAndKeepsTheRest);
    TEST_METHOD(SpellsNamesToMatchTheNewKey);
    TEST_METHOD(MovesABassNoteAfterASlash);
    TEST_METHOD(LeavesAnythingElseUnchanged);
};
