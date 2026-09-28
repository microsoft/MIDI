// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

#include "RtpMidiTestMocks.h"
#include "RtpMidiTestPeer.h"
#include "RtpMidiTestJson.h"

// The service transport DLL, loaded into this process through DllGetClassObject and handed mock
// service interfaces. Remote devices are real protocol engines talking to it over loopback.
//
// The transport keeps process-wide state, as it would in the service, so the class shares one
// transport and one host across its tests. Each test looks only at the endpoints it caused.
class RtpMidiTransportTests
    : public WEX::TestClass<RtpMidiTransportTests>
{
public:

    BEGIN_TEST_CLASS(RtpMidiTransportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD(TestHostDefinedBeforeTheEndpointManagerStarts);
    TEST_METHOD(TestRemoteInvitesHost);
    TEST_METHOD(TestClientConnectsToRemoteHost);
    TEST_METHOD(TestSameNameFromTwoRemotesGetsTwoEndpoints);
    TEST_METHOD(TestRestartedRemoteReplacesItsOldConnection);
    TEST_METHOD(TestHostileConfigurationIsRejected);
    TEST_METHOD(TestDeeplyNestedJsonIsRejected);
    TEST_METHOD(TestEightRemotesWithMidiBothWays);
    TEST_METHOD(TestConnectionChurn);
    TEST_METHOD(TestRemoveHost);

    // who may connect, in RtpMidiApprovalTests.cpp
    TEST_METHOD(TestInvitationIsHeldUntilApprovedOnce);
    TEST_METHOD(TestApproveAlwaysIsRemembered);
    TEST_METHOD(TestApproveUntilRestartBeforeTheRemoteAsks);
    TEST_METHOD(TestDenyOnceRefusesTheWaitingRemote);
    TEST_METHOD(TestDenyAlwaysEndsTheConnectionAndRefusesFromThenOn);
    TEST_METHOD(TestForgettingADecisionAsksAgain);
    TEST_METHOD(TestRememberedDecisionsFromTheConfigurationFile);
    TEST_METHOD(TestDeniedRemoteIsRefusedByAHostWhichAllowsAnyone);
    TEST_METHOD(TestUnrecognizedPolicyRequiresApproval);
    TEST_METHOD(TestApprovalCommandsCheckTheirArguments);
    TEST_METHOD(TestNamesDefaultToThisPcName);

private:
    winrt::Windows::Data::Json::JsonObject Send(std::wstring const& text, HRESULT* result = nullptr);
    winrt::Windows::Data::Json::JsonObject FindHost(std::wstring const& hostId);
    winrt::Windows::Data::Json::JsonObject FindClient(std::wstring const& clientId);

    // A host of the test's own, so its decisions and connections are not shared. Never advertised.
    std::wstring CreateHost(std::wstring const& fields, uint16_t& port);
    void RemoveHost(std::wstring const& hostId);

    winrt::Windows::Data::Json::JsonObject Decide(std::wstring const& verb, std::wstring const& hostId, std::wstring const& remoteName, std::wstring const& scope);
    winrt::Windows::Data::Json::JsonObject FindPending(std::wstring const& hostId, std::wstring const& remoteName);

    IMidiTransport* m_transport{ nullptr };
    IMidiTransportConfigurationManager* m_configuration{ nullptr };
    IMidiEndpointManager* m_endpointManager{ nullptr };

    // never freed: the transport's process-wide state outlives this class and keeps them
    RtpMidiTest::MockDeviceManager* m_deviceManager{ nullptr };
    RtpMidiTest::MockProtocolManager* m_protocolManager{ nullptr };

    std::wstring m_hostId;
    uint16_t m_hostPort{ 0 };
};
