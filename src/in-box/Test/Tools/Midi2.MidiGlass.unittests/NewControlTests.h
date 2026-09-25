// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class NewControlTests : public WEX::TestClass<NewControlTests>
{
public:

    BEGIN_TEST_CLASS(NewControlTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- two axis ----

    TEST_METHOD(ATwoAxisControlSetsAPositionOutright);
    TEST_METHOD(TheSecondAxisReadsBottomToTop);
    TEST_METHOD(AJoystickTakesTwoAxesAndARibbonDoesNot);
    TEST_METHOD(OnlyTheMessagesOnThatAxisAreSent);
    TEST_METHOD(AnAxisWithNoMessagesSendsNothing);

    // ---- keys ----

    TEST_METHOD(AKeyboardOfTwentyFiveHasFifteenWhiteKeys);
    TEST_METHOD(ATouchOnTheLeftEdgeIsTheLowestKey);
    TEST_METHOD(ABlackKeyWinsOverTheWhiteOneBehindIt);
    TEST_METHOD(TheLowerPartOfAWhiteKeyIsAlwaysWhite);
    TEST_METHOD(ATouchOutsideTheKeyboardIsNoKey);
    TEST_METHOD(VelocityRisesTowardTheFrontOfAKey);
    TEST_METHOD(AKeyPlaysItsOwnNoteRatherThanTheRowNumber);

    // ---- the clock ----

    TEST_METHOD(AClockSendsOneWordPerDestination);
    TEST_METHOD(AClockSendsOnceToADeviceNamedTwice);
    TEST_METHOD(AClockSendsNothingWhileTheDeviceIsGone);

    // ---- stops ----

    TEST_METHOD(AListOfStopsIsCountedAsItStands);
    TEST_METHOD(EvenStepsAreCountedFromTheMinimum);
    TEST_METHOD(ASmoothControlHasNoStops);
    TEST_METHOD(StopsAreParsedFromWhateverSeparatorWasTyped);
    TEST_METHOD(OneBadEntryDoesNotEmptyTheList);
    TEST_METHOD(AStopListRoundTripsThroughItsText);
    TEST_METHOD(ExactStopsArePrintedAsThemselves);
    TEST_METHOD(FractionStopsArePrintedAsPercentages);
    TEST_METHOD(TooManyStopsAreNotLabeled);

    // ---- what a control listens for ----

    TEST_METHOD(AnActivityLampTakesAnythingFromItsDevice);
    TEST_METHOD(AnActivityLampCanBeNarrowedToOneChannel);
    TEST_METHOD(AnActivityLampIgnoresAnotherDevice);
    TEST_METHOD(AMessageBindingIsNotLitByActivity);
    TEST_METHOD(AnActivityBindingDoesNotMoveAControl);
};
