// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// Adds and removes MIDI 2.0 loopbacks while applications have them open and are sending through
// them, checking that the service stays up and that only the loopback asked for is touched.
class MidiLoopbackStressTests
    : public WEX::TestClass<MidiLoopbackStressTests>
{
public:

    BEGIN_TEST_CLASS(MidiLoopbackStressTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.LoopbackMidiTransport.dll")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCreateAndRemoveSamePairRepeatedly);
    TEST_METHOD(TestRemoveWhileReceiversConnectedAndSending);
    TEST_METHOD(TestRecreateWhileOldConnectionsStillOpen);
    TEST_METHOD(TestRemoveWhileWinMMPortsOpenAndSending);

    // One side's connections come and go while the other side sends without a break
    TEST_METHOD(TestSideReopenedWhileOtherSideSends);

    // A removal names the loopback by association id, and an id which isn't a MIDI 2.0 loopback's
    // must be refused without touching anything (see GitHub issue #1081)
    TEST_METHOD(TestRemoveWithAssociationIdItDoesNotOwnIsRejected);

    // Removing a loopback removes every endpoint whose instance id starts with its own, so a
    // unique identifier which overlaps another loopback's on the same side is refused
    TEST_METHOD(TestOverlappingUniqueIdsAreRejected);

private:

    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};

};
