// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Novation Launchpad X Programmer mode protocol, Clip/Note surface state machine, and Sequencer snapshot mapping.
class LaunchpadXTests : public WEX::TestClass<LaunchpadXTests>
{
public:
    BEGIN_TEST_CLASS(LaunchpadXTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ProgrammerModeAndLayoutSysExEncodeToValidSysEx7);
    TEST_METHOD(DeviceInquiryRequestAndReplyDecodeAppAndBootloader);
    TEST_METHOD(BulkLedSysExSupportsAllTypesClampsRgbAndChunksAt81);
    TEST_METHOD(ChannelVoiceLedPacketsEncodeChannelsAndStatuses);
    TEST_METHOD(SysEx7RoundTripHandlesEmptyShortMultiPacketAndMalformed);
    TEST_METHOD(InputParserHandlesMidi1AndMidi2PadsButtonsAndReleases);
    TEST_METHOD(EndpointNameMatcherMatchesLpxMidiAndRejectsLpxDaw);
    TEST_METHOD(GridAndSceneCoordinateHelpersMatchProgrammerSpec);

    TEST_METHOD(ClipModePadPressTriggersSlotsAndIgnoresOutOfBounds);
    TEST_METHOD(ClipModeSceneButtonsMapTopDown89To19);
    TEST_METHOD(ClipModeHugeViewportOffsetNeverWrapsAround);
    TEST_METHOD(ClipModeNavigationButtonsScrollViewportWithClamping);
    TEST_METHOD(ClipModeStopAndRecordButtonsEmitExpectedActions);
    TEST_METHOD(ClipModeLedFeedbackEncodesStatesAndDiffsIncrementally);

    TEST_METHOD(NoteModeChromaticAndScaleLayoutsMapPitchesAndRoots);
    TEST_METHOD(NoteModePadPressReleaseAndAftertouchTrackActiveNotes);
    TEST_METHOD(NoteModeOctaveShiftReleasesHeldNotesAndClampsBounds);
    TEST_METHOD(NoteModeLedFeedbackHighlightsRootScaleAndPressedPads);

    TEST_METHOD(ModeSwitchingViaButtonsAndApiReleasesNotesAndRepaints81Leds);
    TEST_METHOD(BuildSurfaceSnapshotMapsSequenceLaunchViewsAndFolders);
};
