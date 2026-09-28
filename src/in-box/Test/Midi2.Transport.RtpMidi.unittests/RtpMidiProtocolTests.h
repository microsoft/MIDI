// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The RTP-MIDI codec, the recovery journal and the session engine. The session tests run two
// engines over a simulated network, with skewed clocks (one about to wrap its 32-bit RTP
// timestamp), a one-way delay and chosen packets dropped. No sockets, so every run is the same.
class RtpMidiProtocolTests
    : public WEX::TestClass<RtpMidiProtocolTests>
{
public:

    BEGIN_TEST_CLASS(RtpMidiProtocolTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // AppleMIDI session messages
    TEST_METHOD(TestInvitationRoundTrip);
    TEST_METHOD(TestInvitationNameHandling);
    TEST_METHOD(TestSynchronizationAndFeedback);

    // RTP-MIDI packets
    TEST_METHOD(TestRtpHeader);
    TEST_METHOD(TestDecodeRunningStatusAndDeltas);
    TEST_METHOD(TestDecodeFirstDeltaAndRealTime);
    TEST_METHOD(TestDecodeSysEx);
    TEST_METHOD(TestDecodeLongHeaderAndJournalOffset);
    TEST_METHOD(TestDecodeRejectsMalformed);
    TEST_METHOD(TestDecoderSurvivesRandomInput);
    TEST_METHOD(TestEncoderBasics);
    TEST_METHOD(TestEncoderLargeSysEx);
    TEST_METHOD(TestEncoderStreamsOpenSysEx);

    // recovery journal
    TEST_METHOD(TestJournalRoundTrip);
    TEST_METHOD(TestJournalParserSkipsAndRejects);

    // session engine
    TEST_METHOD(TestSessionEstablishesAndSynchronizes);
    TEST_METHOD(TestClockFilterRejectsAsymmetricDelay);
    TEST_METHOD(TestPeerStartedSyncIsUsed);
    TEST_METHOD(TestSessionCarriesMidiBothWays);
    TEST_METHOD(TestSendToOneParticipant);
    TEST_METHOD(TestLossRecoveredFromJournal);
    TEST_METHOD(TestLossWithoutJournalSilences);
    TEST_METHOD(TestFeedbackTrimsJournal);
    TEST_METHOD(TestEndSessionAndRejection);
    TEST_METHOD(TestHeldInvitationIsAnsweredLater);
    TEST_METHOD(TestTimeouts);
    TEST_METHOD(TestReinvitationReplacesStaleParticipant);
    TEST_METHOD(TestSessionSurvivesGarbage);
};
