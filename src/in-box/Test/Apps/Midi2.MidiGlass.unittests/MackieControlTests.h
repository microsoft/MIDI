// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class MackieControlTests : public WEX::TestClass<MackieControlTests>
{
public:

    BEGIN_TEST_CLASS(MackieControlTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the functions ----

    TEST_METHOD(EveryFunctionReadsBackFromItsName);
    TEST_METHOD(TheButtonMapIsTheOneDawsExpect);
    TEST_METHOD(EachKindOfControlIsOfferedWhatItCanDo);

    // ---- what goes out ----

    TEST_METHOD(AButtonSendsAPressAndAReleaseAsNoteOns);
    TEST_METHOD(AToggleSendsAWholePressEachTime);
    TEST_METHOD(AFaderSendsPitchBendAndItsTouchNote);
    TEST_METHOD(TheMasterFaderIsTheNinth);
    TEST_METHOD(AVPotSendsTurnsRatherThanPositions);
    TEST_METHOD(TurnsAreCountedWithoutLosingAny);
    TEST_METHOD(APlainRowOnAMackieDeviceSendsNothing);
    TEST_METHOD(AFunctionOnAPlainDeviceSendsNothing);
    TEST_METHOD(FunctionsStayOutOfTheStartupPass);

    // ---- what comes back ----

    TEST_METHOD(TheDawLightsAButtonAndCanMakeItBlink);
    TEST_METHOD(TheDawMovesAFader);
    TEST_METHOD(ALightOnlyAnswersItsOwnDevice);

    // ---- the device's protocol ----

    TEST_METHOD(AMidi1DeviceGetsMidi1Words);
    TEST_METHOD(ASequenceStepFollowsItsDevice);
    TEST_METHOD(TheProtocolAndTheFunctionsSurviveTheFile);
    TEST_METHOD(ANewerProtocolOrFunctionIsKeptAsItWas);

    // ---- switching ----

    TEST_METHOD(SwitchingToMackieTurnsMatchingRowsIntoFunctions);
    TEST_METHOD(SwitchingBackTurnsFunctionsIntoPlainRows);
    TEST_METHOD(SwitchingKeepsExactValuesMeaningful);
    TEST_METHOD(MovingARowToAMackieDeviceMakesItAFunction);
    TEST_METHOD(ANewControlWaitsForAFunction);

    // ---- the starter ----

    TEST_METHOD(TheStarterIsAWholeSurface);
};
