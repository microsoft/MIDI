// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.NetworkMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2NetworkMidiAbstraction);


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager,
    IUnknown* /*midiEndpointProtocolManager*/,
    LPCWSTR ConfigurationJson

)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_transportAbstractionId = AbstractionLayerGUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportAbstractionId;                           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    if (ConfigurationJson != nullptr)
    {
        try
        {
            std::wstring json{ ConfigurationJson };

            if (!json.empty())
            {
                m_JsonObject = json::JsonObject::Parse(json);

                LOG_IF_FAILED(CreateConfiguredEndpoints(json));
            }
        }
        catch (...)
        {
            OutputDebugString(L"Exception processing json for virtual MIDI abstraction");

            // we return S_OK here because otherwise this prevents the service from starting up.
            return S_OK;
        }

    }
    else
    {
        // empty / null is fine. We just continue on.

        OutputDebugString(L"Configuration json is null for virtual MIDI abstraction");

        return S_OK;
    }

    return S_OK;
}




void SwMidiParentDeviceCreateCallback(__in HSWDEVICE /*hSwDevice*/, __in HRESULT /*CreationResult*/, __in_opt PVOID /*pContext*/, __in_opt PCWSTR /* pszDeviceInstanceId */)
{
}



HRESULT
CMidi2NetworkMidiEndpointManager::CreateParentDevice()
{
    return S_OK;
}


_Use_decl_annotations_
HRESULT CMidi2NetworkMidiEndpointManager::CreateConfiguredEndpoints(std::wstring configurationJson)
{
    return S_OK;
}


// this will be called from the runtime endpoint creation interface

HRESULT 
CMidi2NetworkMidiEndpointManager::CreateEndpoint()
{
    return S_OK;
}



HRESULT
CMidi2NetworkMidiEndpointManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}
