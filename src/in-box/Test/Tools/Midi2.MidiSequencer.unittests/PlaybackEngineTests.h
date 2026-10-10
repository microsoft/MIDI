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
    TEST_METHOD(StartingPartWayChasesTheChannelState);
    TEST_METHOD(StartupMessagesGoFirst);
    TEST_METHOD(TheMetronomeClicksOnTheBeat);
    TEST_METHOD(ClockOutSends24PulsesAQuarterNote);
    TEST_METHOD(ADestinationOffsetHandsOverSooner);
    TEST_METHOD(TheThreadPlaysOnItsOwn);
    TEST_METHOD(TheLoopGoesRoundAtItsEnd);
    TEST_METHOD(ANoteHeldOverTheLoopEndEndsThere);
    TEST_METHOD(ALaunchedClipStartsAtTheNextBar);
    TEST_METHOD(StoppingATrackSilencesItFromTheNextBar);
    TEST_METHOD(BackToTimelinePicksUpTheTimeline);
    TEST_METHOD(AGroupCanSpeakADifferentProtocol);
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
