// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Thinning a backlog behind a send speed limit, shared by the Network MIDI 2.0 and RTP-MIDI
// transports and MIDI Patchbay: what is removed, what never is, and what ends a line.
class MidiSendBacklogPrunerTests
    : public WEX::TestClass<MidiSendBacklogPrunerTests>
{
public:

    BEGIN_TEST_CLASS(MidiSendBacklogPrunerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestThresholdAndHowHardToThin);
    TEST_METHOD(TestRampKeepsBothEndsAndEveryOtherStep);
    TEST_METHOD(TestKeepOneInFourAndKeepNone);
    TEST_METHOD(TestTurningPointsAreKept);
    TEST_METHOD(TestRepeatedValuesAreKept);
    TEST_METHOD(TestFewerThanThreePointsChangeNothing);
    TEST_METHOD(TestNoteKeepsTheValuesAroundIt);
    TEST_METHOD(TestOtherChannelsDoNotEndALine);
    TEST_METHOD(TestEachLineIsThinnedOnItsOwn);
    TEST_METHOD(TestEveryMidi1ControllerNumber);
    TEST_METHOD(TestEveryMidi2ControllerNumber);
    TEST_METHOD(TestEveryMidi1ChannelVoiceStatus);
    TEST_METHOD(TestEveryMidi2ChannelVoiceStatus);
    TEST_METHOD(TestFourteenBitControllerIsNotThinned);
    TEST_METHOD(TestSystemExclusiveEndsLinesInItsGroupOnly);
    TEST_METHOD(TestClockAndTextDoNotEndALine);
    TEST_METHOD(TestTimestampsAndStreamMessagesEndEveryLine);
    TEST_METHOD(TestPerNoteLinesAreSeparate);
    TEST_METHOD(TestOnlyMessagesFromTheStartAreTouched);
    TEST_METHOD(TestPartialMessageAtTheEndStays);
    TEST_METHOD(TestRemovedWireBytes);
    TEST_METHOD(TestRepeatedPassesStopAtTheTurningPoints);
    TEST_METHOD(TestTheFirstStepsOfEachRunAlwaysGo);
    TEST_METHOD(TestRepeatedPassesKeepTheFirstStepsOfEachRun);
    TEST_METHOD(TestMackieControlMetersAreNotThinned);
    TEST_METHOD(TestAcceleratingRelativeEncodersAreNotThinned);
    TEST_METHOD(TestMidi1BytesAsRtpMidiQueuesThem);
    TEST_METHOD(TestMidi1BytesThatDoNotAddUpAreLeftAlone);
    TEST_METHOD(TestMatchesASimpleReferenceOnRandomTraffic);
    TEST_METHOD(TestLargeBacklogIsQuick);
};
