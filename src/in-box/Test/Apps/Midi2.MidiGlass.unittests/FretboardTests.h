// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class FretboardTests : public WEX::TestClass<FretboardTests>
{
public:

    BEGIN_TEST_CLASS(FretboardTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- tunings and frets ----

    TEST_METHOD(TuningsPutTheLowStringAtTheBottom);
    TEST_METHOD(EditingAStringMakesTheTuningCustom);
    TEST_METHOD(AFretCountsSemitonesFromTheOpenString);
    TEST_METHOD(TheCapoMovesTheOpenStringAndSilencesTheFretsBehindIt);
    TEST_METHOD(ABanjosShortStringStartsAtTheFifthFret);
    TEST_METHOD(MutedOutOfKeyFretsPlayNothing);
    TEST_METHOD(AStrangersFileIsKeptInsideTheLimits);

    // ---- where things are ----

    TEST_METHOD(StringsRunTopToBottomWithTheStrumAreaOnTheRight);
    TEST_METHOD(AStrumAreaOnTheLeftTurnsTheNeckRound);
    TEST_METHOD(FretsGetCloserUpTheNeck);
    TEST_METHOD(APointFindsItsStringAndFret);
    TEST_METHOD(ASmoothSlideFollowsTheFingerBetweenFrets);
    TEST_METHOD(AStrumCrossesStringsInTheOrderTheFingerMeetsThem);
    TEST_METHOD(AFasterStrumIsLouder);
    TEST_METHOD(PushingAcrossAStringBendsItUpEitherWay);
    TEST_METHOD(TheNeckStripMovesTheFretsShowingAndKeepsTheirCount);

    // ---- one note per string ----

    TEST_METHOD(ATapPlaysAndLiftingEndsItWhileHeld);
    TEST_METHOD(AStringRingsUntilItIsPlayedAgain);
    TEST_METHOD(AHammerOnIsLegatoAndAPullOffGoesBack);
    TEST_METHOD(AFretBehindAHigherOneSoundsNothing);
    TEST_METHOD(WithoutHammerOnsEveryChangeIsANewAttack);
    TEST_METHOD(ASteppedSlideBendsAndAnOffSlideReplays);
    TEST_METHOD(StrummingFretsSilentlyAndPlaysTheFrettedNotes);
    TEST_METHOD(MuteStopsEveryString);
    TEST_METHOD(TwoStringsOnOneNoteShareItOnOneChannel);
    TEST_METHOD(AMutedFretDampsTheString);

    // ---- on the wire ----

    TEST_METHOD(EachStringGetsAChannelOfItsOwn);
    TEST_METHOD(MpeTellsOnlyTheManagerChannelTheSensitivity);
    TEST_METHOD(PerNoteBendStaysOnOneChannel);
    TEST_METHOD(AMidi1RowBendsIn14Bits);

    // ---- buttons, pickers, readouts and the file ----

    TEST_METHOD(CapoAndNeckButtonsChangeOnlyTheFretboardsTheyTarget);
    TEST_METHOD(PadButtonsLeaveFretboardsAlone);
    TEST_METHOD(ATuningPickerSetsAndShowsTheTuning);
    TEST_METHOD(ReadoutsShowTheCapoAndTheTuning);
    TEST_METHOD(AFretboardSurvivesARoundTrip);

    // ---- the editor ----

    TEST_METHOD(ANewFretboardIsAGuitarInStandardTuning);
    TEST_METHOD(TheEditorKeepsAFretboardInsideItsLimits);
    TEST_METHOD(AddingFretboardButtonsWiresThemToTheFretboard);
};
