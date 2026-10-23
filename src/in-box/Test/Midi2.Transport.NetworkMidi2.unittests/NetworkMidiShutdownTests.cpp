// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace NetworkMidiTest;

namespace
{
    constexpr GUID TransportClsid{ NetworkTransportClsid };
}


bool NetworkMidiShutdownTests::ClassSetup()
{
    m_getClassObject = LoadTransportForTest();
    VERIFY_IS_NOT_NULL(m_getClassObject, L"the transport DLL loads and exports DllGetClassObject");

    return m_getClassObject != nullptr;
}

_Use_decl_annotations_
void NetworkMidiShutdownTests::RunShutdown(bool const endpointManagerFirst)
{
    // Never deleted. A transport which keeps a reference releases it at DLL unload, and that must
    // fail this test rather than crash the test process.
    auto& deviceManager = *new StandInDeviceManager();
    auto& protocolManager = *new StandInProtocolManager();

    wil::com_ptr_nothrow<IClassFactory> factory;
    VERIFY_SUCCEEDED(m_getClassObject(TransportClsid, __uuidof(IClassFactory), factory.put_void()));

    wil::com_ptr_nothrow<IMidiTransport> transport;
    VERIFY_SUCCEEDED(factory->CreateInstance(nullptr, __uuidof(IMidiTransport), transport.put_void()));

    wil::com_ptr_nothrow<IMidiTransportConfigurationManager> configurationManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiTransportConfigurationManager), configurationManager.put_void()));
    VERIFY_SUCCEEDED(configurationManager->Initialize(TransportClsid, &deviceManager, nullptr));

    wil::com_ptr_nothrow<IMidiEndpointManager> endpointManager;
    VERIFY_SUCCEEDED(transport->Activate(__uuidof(IMidiEndpointManager), endpointManager.put_void()));
    VERIFY_SUCCEEDED(endpointManager->Initialize(&deviceManager, &protocolManager));

    VERIFY_IS_GREATER_THAN(deviceManager.References(), 1ul, L"the transport took a reference to the device manager");
    VERIFY_IS_GREATER_THAN(protocolManager.References(), 1ul, L"the transport took a reference to the protocol manager");

    if (endpointManagerFirst)
    {
        VERIFY_SUCCEEDED(endpointManager->Shutdown());
        VERIFY_SUCCEEDED(configurationManager->Shutdown());
    }
    else
    {
        VERIFY_SUCCEEDED(configurationManager->Shutdown());
        VERIFY_SUCCEEDED(endpointManager->Shutdown());
    }

    // What the service does next: it lets go of the managers, then destroys its own objects
    endpointManager.reset();
    configurationManager.reset();
    transport.reset();
    factory.reset();

    Log::Comment(String().Format(L"References left: device manager %u, protocol manager %u",
        deviceManager.References(), protocolManager.References()));

    VERIFY_ARE_EQUAL(deviceManager.References(), 1ul, L"the transport released every reference to the device manager");
    VERIFY_ARE_EQUAL(protocolManager.References(), 1ul, L"the transport released every reference to the protocol manager");
}


void NetworkMidiShutdownTests::TestShutdownEndpointManagerFirstReleasesServiceObjects()
{
    RunShutdown(true);
}

void NetworkMidiShutdownTests::TestShutdownConfigurationManagerFirstReleasesServiceObjects()
{
    RunShutdown(false);
}
