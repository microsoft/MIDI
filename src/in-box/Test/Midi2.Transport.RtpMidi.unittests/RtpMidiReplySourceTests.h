// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The table of which local address each remote reached a socket on. Replies go out from that
// address, so a remote left out of it is answered from whatever address Windows picks.
class RtpMidiReplySourceTests
    : public WEX::TestClass<RtpMidiReplySourceTests>
{
public:

    BEGIN_TEST_CLASS(RtpMidiReplySourceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestFollowsTheAddressARemoteReaches);
    TEST_METHOD(TestFullTableMakesRoomForANewRemote);
    TEST_METHOD(TestRemoteStillTalkingIsNotPushedOut);
};
