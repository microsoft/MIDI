// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The sound a track picks before it plays: its program and bank (design section 26).
class TrackSoundTests : public WEX::TestClass<TrackSoundTests>
{
public:

    BEGIN_TEST_CLASS(TrackSoundTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NoProgramChangeMeansNoSound);
    TEST_METHOD(AMidi1ProgramTakesTheBankBeforeIt);
    TEST_METHOD(AMidi2ProgramCarriesItsOwnBank);
    TEST_METHOD(TheLastProgramChangeWins);
    TEST_METHOD(ASoundGoesFirstAndReplacesTheOldOne);
    TEST_METHOD(ClearingTheSoundKeepsTheRest);
    TEST_METHOD(AMidi1DeviceGetsBankSelectThenProgram);
    TEST_METHOD(TheSoundNameIsKeptInTheFile);
};
