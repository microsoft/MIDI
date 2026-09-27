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
    TEST_METHOD(ASoftKeyStillPlaysANoteOn);
    TEST_METHOD(ALightPadPressStillPlaysANoteOn);
    TEST_METHOD(ANoteOnNeverGoesOutAtVelocityZero);

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
    TEST_METHOD(ANoteLampTakesOnlyNotes);
    TEST_METHOD(AControllerLampTakesOnlyControlChanges);
    TEST_METHOD(ATransportLampLatchesOnStartAndClearsOnStop);
    TEST_METHOD(ABeatLampCountsClockMessages);
    TEST_METHOD(AMessageBindingIsNotLitByActivity);
    TEST_METHOD(AnActivityBindingDoesNotMoveAControl);

    // ---- cropping a picture or a video ----

    TEST_METHOD(AFilledPictureCoversTheControl);
    TEST_METHOD(AUniformPictureFitsInsideTheControl);
    TEST_METHOD(AStretchedPictureTakesTheControlsShape);
    TEST_METHOD(ZoomMakesThePictureLarger);
    TEST_METHOD(TheMiddleDecidesWhichSliceIsShown);
    TEST_METHOD(PanningCannotUncoverTheControl);
    TEST_METHOD(ASmallPictureSitsWhereTheMiddleSays);
    TEST_METHOD(AlignmentAndCropShareOneNumberPerAxis);
    TEST_METHOD(AnUndecodedPictureFillsTheControl);

    // ---- the part of a video that plays ----

    TEST_METHOD(ThePartThatPlaysStaysInsideTheFile);
    TEST_METHOD(APartTooShortToPlayIsLengthened);
    TEST_METHOD(AnUnopenedVideoKeepsItsPoints);
    TEST_METHOD(TheBarMapsAcrossThePartThatPlays);
    TEST_METHOD(AVideoTimeReadsLikeAPlayer);
    TEST_METHOD(TheBarSpansOnlyWhatIsShown);

    // ---- wheel and switch ----

    TEST_METHOD(ANewWheelIsAPitchWheel);
    TEST_METHOD(ASwitchPicksThePositionUnderTheFinger);
    TEST_METHOD(ASwitchSendsOnlyTheRowForItsPosition);
    TEST_METHOD(ASwitchSurvivesSavingAndLoading);

    // The step sequencer.
    TEST_METHOD(StepsWalkForwardBackwardAndBothWays);
    TEST_METHOD(RandomStepsStayOnThePattern);
    TEST_METHOD(SwingHoldsBackEverySecondStep);
    TEST_METHOD(AStepsNoteEndsBeforeTheNextStepStarts);
    TEST_METHOD(ANewStepsControlPlaysAnArpeggioOnANoteRow);
    TEST_METHOD(AStepsControlSurvivesSavingAndLoading);
    TEST_METHOD(AStepsFileCannotAskForTooMuch);
};
