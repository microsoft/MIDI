// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Loads the Basic Loopback transport DLL into the test process, the way the service does, and
// hands it a stand-in for the service's device manager. Needs no service and no admin rights.
//
// The stand-in keeps a list of activated instance ids and answers a second activation of one the
// way the service does: S_FALSE and no interface id. The transport used to take that for success
// and build a string from the null id, which crashed the service. Any app could reach it with
// three create calls, or two at once, and a configuration file holding two unique ids that differ
// only in case did it on every service start.
class BasicLoopbackInProcessTests
    : public WEX::TestClass<BasicLoopbackInProcessTests>
{
public:

    BEGIN_TEST_CLASS(BasicLoopbackInProcessTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.BasicLoopbackMidiTransport.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);

    // create {G, "x"}, create {G, "y"}, create {G2, "x"}
    TEST_METHOD(TestReusedAssociationIdIsRefused);

    // the service already has the instance id, so ActivateEndpoint says S_FALSE
    TEST_METHOD(TestEndpointTheServiceAlreadyHasIsAnError);

    // "Loop1" and "loop1" in one configuration file, loaded before the endpoint manager starts
    TEST_METHOD(TestConfigFileUniqueIdsDifferingOnlyInCaseCreateOne);

    // several apps creating the same unique id at the same moment
    TEST_METHOD(TestConcurrentCreatesOfOneUniqueIdCreateOne);

    // the service shuts every endpoint manager down, then every configuration manager
    TEST_METHOD(TestShutdownEndpointManagerFirstReleasesServiceObjects);
    TEST_METHOD(TestShutdownConfigurationManagerFirstReleasesServiceObjects);

private:
    void RunShutdown(_In_ bool const endpointManagerFirst);

    using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

    GetClassObjectFunction m_getClassObject{ nullptr };
};
