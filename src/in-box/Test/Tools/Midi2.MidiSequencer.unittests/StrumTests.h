// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The Strum generator and the piano roll's Strum (strum proposal; Pete, November 5 2026): chords
// voiced the way a guitar, a ukulele or a piano holds them, strokes spread along their curve,
// the same notes whatever windows playback asks for, key switches for muted strokes, the file,
// and a piano roll strum that can be redone and taken back exactly.
class StrumTests : public WEX::TestClass<StrumTests>
{
public:

    BEGIN_TEST_CLASS(StrumTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(CurvesStartAtZeroAndEndAtOne);
    TEST_METHOD(GuitarPlaysOpenAndBarreShapes);
    TEST_METHOD(UkuleleAndPianoVoicings);
    TEST_METHOD(DownStrokeSpreadsLowToHigh);
    TEST_METHOD(UpStrokeHitsTheTopStrings);
    TEST_METHOD(SpreadInMillisecondsFollowsTheTempo);
    TEST_METHOD(LastNoteLandsOnTheBeatAndBassStays);
    TEST_METHOD(StringsRingUntilTheNextStroke);
    TEST_METHOD(RasgueadoAndTremoloFillTheStep);
    TEST_METHOD(KeySwitchesForMutedStrokes);
    TEST_METHOD(SameNotesInAnyWindows);
    TEST_METHOD(FollowsAnotherTrack);
    TEST_METHOD(MakeNotesBakesTheStrum);
    TEST_METHOD(StrumSettingsRoundTrip);
    TEST_METHOD(PianoRollStrumRedoesAndComesBack);
};
