// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class PadGridTests : public WEX::TestClass<PadGridTests>
{
public:

    BEGIN_TEST_CLASS(PadGridTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- where the pads go ----

    TEST_METHOD(ANewGridIsThreeFullRowsOfEight);
    TEST_METHOD(ANewHexGridIsThreeFullRowsOfEight);
    TEST_METHOD(PadsFlowIntoMoreRowsAsTheControlNarrows);
    TEST_METHOD(PadsShrinkRatherThanFallOffAShortControl);
    TEST_METHOD(TheFirstPadIsBottomLeft);
    TEST_METHOD(EverySecondHexRowSitsHalfAPadOver);

    // ---- what each pad plays ----

    TEST_METHOD(RowsAFourthApartRepeatNotes);
    TEST_METHOD(ZeroCarriesEachRowOnFromTheLast);
    TEST_METHOD(TwoRowsUpOnWickiHaydenIsAnOctave);
    TEST_METHOD(AMajorChordOnTheHarmonicTableIsThreePadsThatTouch);
    TEST_METHOD(APadOffTheEndOfTheNoteRangePlaysNothing);

    // ---- which pad a finger is on ----

    TEST_METHOD(ATouchInTheGapBelongsToTheNearerPad);
    TEST_METHOD(ATouchPastAShortRowIsNoPad);
    TEST_METHOD(ATouchNearAHexagonsPointIsThatHexagon);

    // ---- the pitch under a sliding finger ----

    TEST_METHOD(AFingerNearTheMiddleOfAPadIsInTune);
    TEST_METHOD(ThePitchMeetsTheNextPadAtTheEdgeBetweenThem);
    TEST_METHOD(TheEndOfARowHoldsItsOwnNote);

    // ---- keys and names ----

    TEST_METHOD(TheRootHasARoleOfItsOwn);
    TEST_METHOD(WithNoKeyEveryPadIsInIt);
    TEST_METHOD(AMinorKeyHoldsItsFlatThird);
    TEST_METHOD(FlatKeysAreWrittenWithFlats);

    // ---- fingers and the notes they hold ----

    TEST_METHOD(APressPlaysANoteAndAReleaseEndsIt);
    TEST_METHOD(TwoFingersPlayAChord);
    TEST_METHOD(ASlideWithGlideOffEndsOneNoteBeforeStartingTheNext);
    TEST_METHOD(APortamentoSlideNamesTheNoteItCameFrom);
    TEST_METHOD(AReleaseAfterASlideEndsTheNoteTheFingerIsOn);
    TEST_METHOD(TwoFingersOnOneNoteShareIt);
    TEST_METHOD(AGlideNeverStealsANoteSomebodyElseIsHolding);
    TEST_METHOD(ABendingNoteStartsInTuneAtItsOwnRange);
    TEST_METHOD(ABendFollowsTheFingerAndIgnoresTinyMoves);
    TEST_METHOD(ABendIsHeldAtItsRange);
    TEST_METHOD(ReleasingEverythingEndsEveryNote);

    // ---- what goes on the wire ----

    TEST_METHOD(PortamentoIsControlChange84CarryingTheNote);
    TEST_METHOD(APerNoteBendIsCenteredAtHalfScale);
    TEST_METHOD(APerNoteBendOnlyGoesToMidi2Rows);
    TEST_METHOD(TheBendRangeIsRegisteredControllerSeven);

    // ---- the file and the editor ----

    TEST_METHOD(PadsSurviveARoundTrip);
    TEST_METHOD(PadsFromAFileAreBounded);
    TEST_METHOD(SwitchingBetweenSquareAndHexCarriesTheStartingLayout);
};
