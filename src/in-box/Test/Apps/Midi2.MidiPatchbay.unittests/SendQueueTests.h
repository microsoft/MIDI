// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The queue in front of a throttle: messages wait for the speed, and a backlog that falls far
// behind has the steps in the middle of its controller sweeps thinned out.
class SendQueueTests : public WEX::TestClass<SendQueueTests>
{
public:

    BEGIN_TEST_CLASS(SendQueueTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ABackedUpSweepIsThinnedAndEndsWhereItShould);
    TEST_METHOD(AFarBehindSweepKeepsItsFirstStepsAndItsEnds);
    TEST_METHOD(NothingIsThinnedWhenSkippingIsTurnedOff);
    TEST_METHOD(NothingIsThinnedWithoutASpeed);
    TEST_METHOD(NothingIsThinnedWhileTheBacklogIsShort);
    TEST_METHOD(NotesAndTheValuesAroundThemAlwaysGo);
    TEST_METHOD(MessagesKeepTheirTimestamps);
    TEST_METHOD(SystemExclusiveIsNeverThinned);
};
