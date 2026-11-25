// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// What the connection monitor windows get from routing: a copy of each message along a watched
// link, through a buffer that never makes routing wait.
class MessageWatchTests : public WEX::TestClass<MessageWatchTests>
{
public:

    BEGIN_TEST_CLASS(MessageWatchTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(MessagesComeOutInOrderAsTheyWentIn);
    TEST_METHOD(AFullBufferDropsAndCounts);
    TEST_METHOD(ANewLeaseNeverSeesTheLastWindowsMessages);
    TEST_METHOD(ManyThreadsPutInAtOnceWithNothingLostOrTorn);
    TEST_METHOD(AFanCopiesToEveryWindowWatching);
    TEST_METHOD(ClockAndActiveSensingAreRecognized);
    TEST_METHOD(WordsAndTimesReadTheWayTheMonitorShowsThem);
    TEST_METHOD(TheHistoryKeepsTheNewestAndFindsRowsByNumber);
    TEST_METHOD(RoutingHandsOverWhatGoesAlongEachLink);
    TEST_METHOD(ATraceSaysWhatWentAlongEachLink);
};
