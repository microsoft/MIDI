// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Custom MIDI 1.0 port names in an endpoint customization: which ports an update clears, and what
// applying it does to the endpoint's name table. Pure tests, so they need no driver and no service.
//
// An empty name means "go back to the generated name". The parser leaves an empty name out, so
// before these changes a cleared port kept its old name on the running endpoint until the service
// restarted, and the next save read that old name back and stored it again.
class MidiPortNameCustomizationTests
    : public WEX::TestClass<MidiPortNameCustomizationTests>
{
public:

    BEGIN_TEST_CLASS(MidiPortNameCustomizationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // reading which ports an update clears
    TEST_METHOD(TestEmptyPortNameIsReadAsCleared);
    TEST_METHOD(TestPortNamedElsewhereInTheListIsNotCleared);
    TEST_METHOD(TestMalformedPortEntriesAreIgnored);
    TEST_METHOD(TestNoPortListMeansNothingIsCleared);

    // the ungated parser has to keep leaving empty names out, so a rollback is a rollback
    TEST_METHOD(TestParserStillLeavesEmptyPortNamesOut);

    // applying an update to the endpoint's name table
    TEST_METHOD(TestNamedPortsAreSetAndOthersAreLeftAlone);
    TEST_METHOD(TestClearedPortLosesItsCustomName);
    TEST_METHOD(TestClearedDestinationLeavesTheSourceAlone);
    TEST_METHOD(TestRemovalClearsEveryPortName);
    TEST_METHOD(TestClearingAGroupTheEndpointLacksIsHarmless);
};
