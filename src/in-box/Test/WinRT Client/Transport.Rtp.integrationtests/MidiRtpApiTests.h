// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tests for the RTP-MIDI API in Windows.Devices.Midi2.dll.
//
// The configuration tests need nothing but the SDK. The rest go through the running service, and
// skip when the RTP-MIDI transport is not installed. A remote device is played by the transport
// tests' RTP-MIDI peer on loopback, so no second machine or network is needed.
class MidiRtpApiTests
    : public WEX::TestClass<MidiRtpApiTests>
{
public:

    BEGIN_TEST_CLASS(MidiRtpApiTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    // Configuration objects, without the service
    TEST_METHOD(TestConstantsMatchRtpMidi);
    TEST_METHOD(TestHostCreationConfigDefaults);
    TEST_METHOD(TestHostCreationConfigJson);
    TEST_METHOD(TestClientConnectConfigJson);
    TEST_METHOD(TestRemovalConfigJson);
    TEST_METHOD(TestKnownClientsConfigWritesBothLists);
    TEST_METHOD(TestCommandConfigsHaveNothingToSave);
    TEST_METHOD(TestNullConfigsAreRejected);

    // Through the service
    TEST_METHOD(TestCreateStopStartRemoveHost);
    TEST_METHOD(TestHostWithNoNameUsesThisPcName);
    TEST_METHOD(TestEntriesWhichDoNotExistAreReported);
    TEST_METHOD(TestClientWithNoRemoteIsRejected);
    TEST_METHOD(TestListsAreReadable);
    TEST_METHOD(TestApproveOnceThenDisconnectTheRemote);
    TEST_METHOD(TestDenyAlwaysIsAKnownClientUntilForgotten);

    // A new send speed limit applies to a running host without restarting it
    TEST_METHOD(TestHostSendSpeedLimitChangesWhileRunning);

    // What is saved in the configuration file. Saved only, never sent to the service.
    TEST_METHOD(TestSavedHostFollowsSavedChanges);
    TEST_METHOD(TestSavingKnownClientsForUnsavedHostIsRefused);
    TEST_METHOD(TestSavedClientFollowsSavedChanges);

    // Limiting a host to one network adapter, and what it does while that adapter is missing
    TEST_METHOD(TestHostCreationConfigNetworkAdapterJson);
    TEST_METHOD(TestSavedHostKeepsItsNetworkAdapter);
    TEST_METHOD(TestHostWaitsForAMissingNetworkAdapter);
    TEST_METHOD(TestHostFallsBackWhenItsNetworkAdapterIsMissing);
    TEST_METHOD(TestHostStartsOnItsNetworkAdapter);

private:
    MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
    std::vector<winrt::guid> m_createdHosts{};

    winrt::guid CreateHost(_In_ std::wstring const& name, _In_ MidiRtpRemoteClientPolicy const policy);
    void RemoveCreatedHosts();
};
