// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The parts of the app that work on the model without a window: edits with their undo, recording
// takes, and the rows the main window draws.
class AppModelTests : public WEX::TestClass<AppModelTests>
{
public:

    BEGIN_TEST_CLASS(AppModelTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ANewTrackAfterAnOpenFolderGoesInside);
    TEST_METHOD(AFolderCantMoveIntoItself);
    TEST_METHOD(MovingTracksUpAndDownStaysInTheirList);
    TEST_METHOD(UnusedClipsAreRemovedAndUsedOnesKept);
    TEST_METHOD(UndoPutsTheTrackTreeBack);
    TEST_METHOD(UndoPutsARemovedClipBack);
    TEST_METHOD(ClipSettingsUndoLeavesTheNotes);
    TEST_METHOD(SnapGoesToTheNearestLine);
    TEST_METHOD(ANewTrackGetsAFreeChannel);

    TEST_METHOD(RecordedMidi1NotesBecomeMidi2Notes);
    TEST_METHOD(AHeldNoteEndsWhereTheTakeStops);
    TEST_METHOD(ATakeStartsAndEndsOnBars);
    TEST_METHOD(TheRecordFilterLeavesThingsOut);
    TEST_METHOD(ARepeatedNoteOnRestartsTheNote);

    TEST_METHOD(PinnedRowsGoToTheTop);
    TEST_METHOD(AClosedFolderHidesItsTracks);
    TEST_METHOD(RowsAreFoundByPosition);

    TEST_METHOD(TheSampleIsWholeAndSurvivesSaving);
};
