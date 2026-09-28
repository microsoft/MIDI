// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The multicast DNS helpers: matching a connection's address to an advertised host name, and
// the follow-up announcements that work around the Windows responder's single announcement.
// Made-up advertisements and a recorder in place of the network, so nothing is sent.
class RtpMidiDnssdTests
    : public WEX::TestClass<RtpMidiDnssdTests>
{
public:

    BEGIN_TEST_CLASS(RtpMidiDnssdTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestHostNameForAddress);
    TEST_METHOD(TestAnnouncementPacket);
    TEST_METHOD(TestAnnouncementLeavesOutBadLabels);
    TEST_METHOD(TestAnnouncementSplitsAcrossPackets);

    TEST_METHOD(TestAnnouncerRepeatsTwice);
    TEST_METHOD(TestAnnouncerNeverRepeatsAWithdrawnHost);
    TEST_METHOD(TestAnnouncerTimesABurstOfRegistrations);
    TEST_METHOD(TestAnnouncerSendsNothingOnceStopped);
};
