// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Merging and splitting clips and tracks (SequenceMerge.h).
class SequenceMergeTests : public WEX::TestClass<SequenceMergeTests>
{
public:

    BEGIN_TEST_CLASS(SequenceMergeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(OverlappingSameNotesBecomeOneLongerNote);
    TEST_METHOD(MergedClipsKeepTheirSpacing);
    TEST_METHOD(MergingALoopingClipCopiesEachPass);
    TEST_METHOD(SplittingANotesClipCutsItInTwo);
    TEST_METHOD(SplittingAGeneratorKeepsItPlayingOnTheLeft);
    TEST_METHOD(SplittingOutsideAPlacementDoesNothing);
    TEST_METHOD(MergingATrackKeepsTheDestinationsSettings);
    TEST_METHOD(MergingATrackMergesWhereBothPlay);
    TEST_METHOD(MergingATrackFillsAndMergesSlots);
    TEST_METHOD(GhostNotesComeFromUnmutedTracksOnly);
};
