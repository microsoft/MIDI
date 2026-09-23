// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSequencePlayerTests
    : public WEX::TestClass<MidiSequencePlayerTests>
{
    BEGIN_TEST_CLASS(MidiSequencePlayerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"Description", L"Playing a sequence through the public API")
    END_TEST_CLASS()

    // Everything here plays to a diagnostic loopback rather than to a synth, so the tests make no
    // sound and nothing has to be listened to. Every file used is a few seconds at most.

    // ---- getting a player ----
    TEST_METHOD(BorrowsAConnectionAndLeavesItOpen);
    TEST_METHOD(OpensAndOwnsAConnectionForAnEndpoint);
    TEST_METHOD(RefusesAnEndpointThatDoesNotExist);
    TEST_METHOD(ClosingTwiceIsHarmless);

    // ---- transport ----
    TEST_METHOD(PlaysAShortFileToTheEnd);
    TEST_METHOD(ReportsPositionWhilePlaying);
    TEST_METHOD(PausesAndResumes);
    TEST_METHOD(StopsAndRewinds);
    TEST_METHOD(SeeksByTickAndByTime);
    TEST_METHOD(RaisesPlaybackEnded);

    // ---- what reaches the wire ----
    TEST_METHOD(SendsNotesToTheEndpoint);
    TEST_METHOD(SilencesEverythingOnStop);
    TEST_METHOD(SilencesHangingNotesAtTheEndOfAFile);
    TEST_METHOD(MutingATrackStopsItsNotes);
    TEST_METHOD(SoloingATrackSilencesTheOthers);

    // ---- routing ----
    TEST_METHOD(RoundTripsTrackRouting);
    TEST_METHOD(RoutingMuteAgreesWithTrackMute);
    TEST_METHOD(RoutesATrackToADifferentGroup);
    TEST_METHOD(RoutesATrackToADifferentChannel);
    TEST_METHOD(RoutesTwoTracksToTwoEndpointsAtOnce);
    TEST_METHOD(ClearingRoutingSendsTheTrackBackToThePlayerConnection);
    TEST_METHOD(SilencesEveryEndpointATrackWasRoutedTo);

    // ---- misuse ----
    TEST_METHOD(TransportCallsWithoutASequenceAreHarmless);
    TEST_METHOD(SettingANullSequenceClearsIt);
    TEST_METHOD(OutOfRangeTrackIndexesAreHarmless);
    TEST_METHOD(SeekingBeyondTheEndIsClamped);
};
