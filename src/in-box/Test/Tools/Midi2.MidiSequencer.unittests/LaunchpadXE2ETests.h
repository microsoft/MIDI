// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Opaque-box end-to-end tests (Tiers 1-4) for Novation Launchpad X integration
// in Windows MIDI Sequencer, covering Clip mode, Note mode, mode switching,
// protocol encoding/decoding, and PlaybackEngine/RecordingTake integration
// without physical hardware.
class LaunchpadXE2ETests : public WEX::TestClass<LaunchpadXE2ETests>
{
public:

    BEGIN_TEST_CLASS(LaunchpadXE2ETests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // Tier 1: Feature Coverage (Isolated Happy Paths, F1..F8, 45 matrix cases)
    TEST_METHOD(T1_F1_ProtocolAndSysEx7Codec);
    TEST_METHOD(T1_F2_ProgrammerModeGridAndButtons);
    TEST_METHOD(T1_F3_ClipModeSurfaceAndNavigation);
    TEST_METHOD(T1_F4_NoteModeScaleLayout);
    TEST_METHOD(T1_F5_ClipNoteModeSwitching);
    TEST_METHOD(T1_F6_EndpointTransportContracts);
    TEST_METHOD(T1_F7_SequencerRuntimeIntegration);
    TEST_METHOD(T1_F8_SettingsConfigInvariants);

    // Tier 2: Boundary, Corner & Negative Cases (F1..F8, 44 matrix cases)
    TEST_METHOD(T2_F1_ProtocolBoundariesAndMalformedUmp);
    TEST_METHOD(T2_F2_CoordinateAndButtonBoundaries);
    TEST_METHOD(T2_F3_ClipModeViewportAndEmptyBounds);
    TEST_METHOD(T2_F4_NoteModeOctaveScaleAndReleaseBounds);
    TEST_METHOD(T2_F5_ModeSwitchRapidAndMidHoldBounds);
    TEST_METHOD(T2_F6_EndpointNameAndInquiryRejections);
    TEST_METHOD(T2_F7_RuntimeNullTrackAndKeyFallbacks);
    TEST_METHOD(T2_F8_ConfigValidationAndLongStrings);

    // Tier 3: Pairwise Cross-Feature Combinations (12 matrix cases)
    TEST_METHOD(T3_01_ClipAndPlaybackQuantizedLaunch);
    TEST_METHOD(T3_02_ClipAndPlaybackOverwritePendingLaunch);
    TEST_METHOD(T3_03_ClipAndSlotRecordingLifecycle);
    TEST_METHOD(T3_04_NoteAndSongKeyAtDynamicKeyChange);
    TEST_METHOD(T3_05_NoteAndSongKeyAtInScaleOctaveShift);
    TEST_METHOD(T3_06_ModeSwitchWithHeldNotesAndActivePlayback);
    TEST_METHOD(T3_07_ModeSwitchDuringSlotRecording);
    TEST_METHOD(T3_08_ViewportScrollAndOffScreenSceneLaunch);
    TEST_METHOD(T3_09_ViewportScrollAndPartialBoundaryWindow);
    TEST_METHOD(T3_10_SysEx7AndChannelVoiceMixedFeedback);
    TEST_METHOD(T3_11_NoteModePolyAftertouchAndTrackMuteSolo);
    TEST_METHOD(T3_12_DeviceInquiryModeSwitchAndDiffCache);

    // Tier 4: Real-World Application Workloads (5 end-to-end scenarios)
    TEST_METHOD(T4_01_LiveClipLaunchingSet);
    TEST_METHOD(T4_02_ScaleLockedMelodyRecordingAndPlayback);
    TEST_METHOD(T4_03_MidSetClipNotePerformanceSwitching);
    TEST_METHOD(T4_04_MultiTrackPagedSessionNavigation);
    TEST_METHOD(T4_05_ReconnectBootloaderAndErrorRecovery);
};
