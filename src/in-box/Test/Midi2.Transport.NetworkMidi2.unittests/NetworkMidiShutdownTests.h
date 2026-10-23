// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Loads the transport DLL into the test process, the way the service does, and hands it stand-ins
// for the service objects. Needs no service, no admin rights and no network peer.
//
// The service destroys its own objects before it exits, and the transport DLL is unloaded after
// that. Any reference the transport still holds at that point is released into freed memory,
// which crashed the service on every stop while a Network MIDI 2.0 host or client was running.
// These tests shut the transport down the way the service does and check every reference it was
// given has been handed back.
class NetworkMidiShutdownTests
    : public WEX::TestClass<NetworkMidiShutdownTests>
{
public:

    BEGIN_TEST_CLASS(NetworkMidiShutdownTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.NetworkMidiTransport.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);

    // The order the service uses: every endpoint manager, then every configuration manager
    TEST_METHOD(TestShutdownEndpointManagerFirstReleasesServiceObjects);

    TEST_METHOD(TestShutdownConfigurationManagerFirstReleasesServiceObjects);

private:
    void RunShutdown(_In_ bool const endpointManagerFirst);

    using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

    GetClassObjectFunction m_getClassObject{ nullptr };
};
