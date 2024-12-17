// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::Initialize(
    GUID transportId, 
    IMidiDeviceManagerInterface* deviceManagerInterface, 
    IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface)
{
    TraceLoggingWrite(
        MidiSrvTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(deviceManagerInterface);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    m_TransportGuid = transportId;

    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize());
    m_MidiSrv = std::move(midiSrv);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::UpdateConfiguration(LPCWSTR configurationJson, LPWSTR* responseJson)
{
    TraceLoggingWrite(
        MidiSrvTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Entering UpdateConfiguration", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(configurationJson, "config json"),
        TraceLoggingPointer(responseJson, "Response pointer")
        );

    RETURN_HR_IF_NULL(CO_E_NOTINITIALIZED, m_MidiSrv);
    return m_MidiSrv->UpdateConfiguration(configurationJson, responseJson);
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetTransportList(LPWSTR* transportListJson)
{ 
    TraceLoggingWrite(
        MidiSrvTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(CO_E_NOTINITIALIZED, m_MidiSrv);
    return m_MidiSrv->GetTransportList(transportListJson);
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetTransformList(LPWSTR* transformListJson)
{
    TraceLoggingWrite(
        MidiSrvTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(CO_E_NOTINITIALIZED, m_MidiSrv);
    return m_MidiSrv->GetTransformList(transformListJson);
}

HRESULT
CMidi2MidiSrvConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    if (m_MidiSrv)
    {
        RETURN_IF_FAILED(m_MidiSrv->Shutdown());
        m_MidiSrv.reset();
    }

    return S_OK;
}

