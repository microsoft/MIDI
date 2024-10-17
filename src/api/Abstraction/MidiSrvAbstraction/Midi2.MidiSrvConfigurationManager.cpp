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
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(deviceManagerInterface);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    m_abstractionGuid = transportId;

    return S_OK;

}





_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::UpdateConfiguration(LPCWSTR configurationJson, LPWSTR* response)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, response);
    RETURN_HR_IF_NULL(E_INVALIDARG, configurationJson);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvUpdateConfiguration(bindingHandle.get(), configurationJson, response));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetTransportList(LPWSTR* transportListJson)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, transportListJson);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvGetTransportList(bindingHandle.get(), transportListJson));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetTransformList(LPWSTR* transformListJson)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, transformListJson);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvGetTransformList(bindingHandle.get(), transformListJson));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
                return S_OK;
        }());

    return S_OK;
}






HRESULT
CMidi2MidiSrvConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    return S_OK;
}

