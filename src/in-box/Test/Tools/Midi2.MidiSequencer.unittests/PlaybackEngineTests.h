// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The playback engine, driven by a fake clock, sending to a recorder.
class PlaybackEngineTests : public WEX::TestClass<PlaybackEngineTests>
{
public:

    BEGIN_TEST_CLASS(PlaybackEngineTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NotesGoOutAtTheirTimes);
    TEST_METHOD(OnlyTheLookAheadIsHandedOver);
    TEST_METHOD(AMuteIsHeardAtTheNextSweep);
    TEST_METHOD(SoloLeavesOnlyTheSoloedTracks);
    TEST_METHOD(AMidi1DestinationGetsMidi1);
    TEST_METHOD(StopEndsWhatItStarted);
    TEST_METHOD(ARestartDoesntCutTheNewNotes);
    TEST_METHOD(AMidi1NoteEndsWithOneWord);
    TEST_METHOD(StartingPartWayChasesTheChannelState);
    TEST_METHOD(StartupMessagesGoFirst);
    TEST_METHOD(TheMetronomeClicksOnTheBeat);
    TEST_METHOD(ClockOutSends24PulsesAQuarterNote);
    TEST_METHOD(ClockStopFollowsTheLastPulse);
    TEST_METHOD(FollowingPlaysAtTheIncomingTempo);
    TEST_METHOD(FollowingWaitsForTheFirstPulse);
    TEST_METHOD(FollowingStopsWithTheClock);
    TEST_METHOD(FollowingStartsWhereTheSongPositionSays);
    TEST_METHOD(ClockOutputsSendOnlyWhatTheyAskFor);
    TEST_METHOD(ClockOutputsCanSendTheTempo);
    TEST_METHOD(TheLoopTellsClockFollowersWhereItWent);
    TEST_METHOD(TimeCodeCountsFromTheTopOfTheSequence);
    TEST_METHOD(TimeCodeGoesBackWithTheLoop);
    TEST_METHOD(TimeCodeStartsPartWayAtThatTime);
    TEST_METHOD(ADestinationOffsetSendsThatMuchEarly);
    TEST_METHOD(TheThreadPlaysOnItsOwn);
    TEST_METHOD(TheLoopGoesRoundAtItsEnd);
    TEST_METHOD(ANoteHeldOverTheLoopEndEndsThere);
    TEST_METHOD(ALaunchedClipStartsAtTheNextBar);
    TEST_METHOD(ALaunchLightsOnlyItsSlot);
    TEST_METHOD(StoppingATrackSilencesItFromTheNextBar);
    TEST_METHOD(AClipCanStartAtAnExactPlace);
    TEST_METHOD(BackToTimelinePicksUpTheTimeline);
    TEST_METHOD(AGroupCanSpeakADifferentProtocol);
    TEST_METHOD(LanesStartWhereTheyAreAtThePlayhead);
    TEST_METHOD(TheLoopSendsLaneValuesAgain);
    TEST_METHOD(AMidi1DestinationGetsALaneAsMidi1);
    TEST_METHOD(AnEditWhilePlayingSendsTheNewValue);
};

class MessageTranslationTests : public WEX::TestClass<MessageTranslationTests>
{
public:

    BEGIN_TEST_CLASS(MessageTranslationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(Midi2NotesAndControllersBecomeMidi1);
    TEST_METHOD(ProgramWithBankAndRegisteredControllersExpand);
    TEST_METHOD(PerNoteMessagesAreDroppedForMidi1);
    TEST_METHOD(Midi1BecomesMidi2);
};
