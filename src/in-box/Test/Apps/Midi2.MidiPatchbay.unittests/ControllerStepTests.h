// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The game controller steps: readings, Learn, the messages a button and a stick send, and the
// file format that carries them.
class ControllerStepTests : public WEX::TestClass<ControllerStepTests>
{
public:

    BEGIN_TEST_CLASS(ControllerStepTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- readings and Learn ----
    TEST_METHOD(XInputReadingsRunFromZeroToOne);
    TEST_METHOD(EveryControlAndMessageHasItsOwnKey);
    TEST_METHOD(LearnFindsTheControlThatMoved);

    // ---- the words ----
    TEST_METHOD(ANoteGoesOnAndOffAsMidi2);
    TEST_METHOD(ANoteCanGoAsMidi1);
    TEST_METHOD(AValueUsesTheWholeMidi2Range);
    TEST_METHOD(AProgramChangeSendsNoBank);
    TEST_METHOD(StartContinueAndStopAreSystemMessages);
    TEST_METHOD(TypedWordsMustBeOneWholeMessage);

    // ---- a button ----
    TEST_METHOD(APressSendsAndLettingGoEndsIt);
    TEST_METHOD(HoldingSendsTheHeldMessageAndRepeats);
    TEST_METHOD(TapOrHoldSendsOnlyOne);
    TEST_METHOD(AToggleSwitchesOnEachPress);
    TEST_METHOD(NextAndPreviousShareOnePlace);
    TEST_METHOD(ReleaseEndsWhatIsStillOn);

    // ---- a stick or trigger ----
    TEST_METHOD(ADeadZoneAndAShapeChangeTheValue);
    TEST_METHOD(AStickSendsOnlyWhenItMovesAStep);
    TEST_METHOD(TheIntervalHoldsBackButNeverLosesTheLastMove);
    TEST_METHOD(AStickGoesBackToRestWhenItStops);

    // ---- the file ----
    TEST_METHOD(ControllerSettingsSurviveTheFile);
    TEST_METHOD(BadValuesInTheFileFallBack);
    TEST_METHOD(OnlyTheControllerAndControlRestartAStep);
    TEST_METHOD(AFileWrittenTheWayTheGuideSaysReads);
};
