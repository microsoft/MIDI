// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Loads the synthesizer transport DLL into the test process, the way the service does, and hands
// it stand-ins for the service objects. Needs no service, no admin rights and no audio device:
// nothing here connects a client, so no sound set is read and no audio device is opened.
//
// The service destroys its own objects before it exits, and the transport DLL is unloaded after
// that. A reference the transport still holds then is released into freed memory. The service
// shuts down every endpoint manager and then every configuration manager, and it never shuts down
// an endpoint manager whose Initialize failed, so the configuration manager has to be able to
// clean up by itself.
class MidiSynthShutdownTests
    : public WEX::TestClass<MidiSynthShutdownTests>
{
public:

    BEGIN_TEST_CLASS(MidiSynthShutdownTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.MidiSynthTransport.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);

    TEST_METHOD(TestShutdownEndpointManagerFirstReleasesServiceObjects);
    TEST_METHOD(TestShutdownConfigurationManagerFirstReleasesServiceObjects);

    // The service already has the synthesizer's instance id, so ActivateEndpoint says S_FALSE with
    // no interface id. That used to be taken for success and crash the service.
    TEST_METHOD(TestEndpointTheServiceAlreadyHasFailsInitializeCleanly);

private:
    enum class ShutdownOrder
    {
        EndpointManagerFirst,
        ConfigurationManagerFirst,
    };

    void RunShutdown(_In_ ShutdownOrder const order);

    using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

    GetClassObjectFunction m_getClassObject{ nullptr };
};
