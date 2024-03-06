// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::Initialize(
    GUID AbstractionId,
    IUnknown* MidiDeviceManager,
    IUnknown* MidiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(MidiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);
    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_abstractionId = AbstractionId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection,
    BOOL IsFromConfigurationFile,
    BSTR* Response
)
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ConfigurationJsonSection, "json")
    );

    UNREFERENCED_PARAMETER(IsFromConfigurationFile);



    // if we're passed a null or empty json, we just quietly exit
    if (ConfigurationJsonSection == nullptr) return S_OK;

    json::JsonObject jsonObject;

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);


    // TODO: All your json parsing config stuff. Build any device table entries from it, etc.
    // See Midi2.LoopbackMidiAbstraction for an example

    // The runtime json sent up from the client in the case of network MIDI may include
    // an IP address, port, password, etc. You can assume there can be a UI on the client
    // for gathering that information and then sending it up. The API just needs to know
    // what is needed.


    // the response object should include anything the client needs to be able to use
    // the endpoint, if the creation was successful. Typically, this is just an SWD 
    // interface id
    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return E_NOTIMPL;

}


HRESULT
CMidi2NetworkMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );




    return S_OK;
}

