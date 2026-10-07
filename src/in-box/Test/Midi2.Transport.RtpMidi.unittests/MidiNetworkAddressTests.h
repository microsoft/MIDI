// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Which of a remote's addresses the Network MIDI 2.0 and RTP-MIDI clients try, and in what order.
// The order itself is Windows', so the sorting tests check what can be known on any PC: both
// orders given come back the same, and nothing comes back that was not given.
class MidiNetworkAddressTests
    : public WEX::TestClass<MidiNetworkAddressTests>
{
public:

    BEGIN_TEST_CLASS(MidiNetworkAddressTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestSortDoesNotDependOnTheOrderGiven);
    TEST_METHOD(TestSortReturnsTheTextAsGiven);
    TEST_METHOD(TestSortLeavesOutWhatCannotBeUsed);
    TEST_METHOD(TestSortKeepsAnAddressItCannotPlace);

    TEST_METHOD(TestResolveFindsLocalhost);
    TEST_METHOD(TestResolveReturnsAnAddressAsItIs);
    TEST_METHOD(TestResolveGivesUpOnceStopped);

    TEST_METHOD(TestChooseNothingFromNoAddresses);
    TEST_METHOD(TestChooseMovesOnForEachUnansweredAttempt);
    TEST_METHOD(TestChooseStartsWhereTheLastSessionOpened);
    TEST_METHOD(TestChooseIgnoresAConnectedAddressNoLongerListed);
    TEST_METHOD(TestNextAddressIsUntriedUntilEachHadATurn);
};
