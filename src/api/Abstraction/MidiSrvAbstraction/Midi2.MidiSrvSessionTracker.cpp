// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


HRESULT
CMidi2MidiSrvSessionTracker::Initialize()
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

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::AddClientSession(
    GUID sessionId,
    LPCWSTR sessionName
)
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
    RETURN_HR_IF_NULL(E_INVALIDARG, sessionName);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvRegisterSession(bindingHandle.get(), sessionId, sessionName, &m_contextHandle));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            
            return S_OK;
        }());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::UpdateClientSessionName(
    GUID sessionId,
    LPCWSTR sessionName
)
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
    RETURN_HR_IF_NULL(E_INVALIDARG, sessionName);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvUpdateSessionName(bindingHandle.get(), m_contextHandle, sessionId, sessionName));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::RemoveClientSession(
    GUID sessionId
)
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

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvDeregisterSession(bindingHandle.get(), m_contextHandle, sessionId));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

            return S_OK;
        }());

    return S_OK;
}

//_Use_decl_annotations_
//HRESULT
//CMidi2MidiSrvSessionTracker::AddClientEndpointConnection(
//    GUID sessionId,
//    LPCWSTR sonnectionEndpointInterfaceId
//)
//{
//    UNREFERENCED_PARAMETER(sessionId);
//    UNREFERENCED_PARAMETER(sonnectionEndpointInterfaceId);
//
//    return S_OK;
//}
//
//_Use_decl_annotations_
//HRESULT
//CMidi2MidiSrvSessionTracker::RemoveClientEndpointConnection(
//    GUID sessionId,
//    LPCWSTR sonnectionEndpointInterfaceId
//)
//{
//    UNREFERENCED_PARAMETER(sessionId);
//    UNREFERENCED_PARAMETER(sonnectionEndpointInterfaceId);
//
//    return S_OK;
//}


//_Use_decl_annotations_
//HRESULT
//CMidi2MidiSrvSessionTracker::GetSessionList(
//    LPSAFEARRAY* sessionDetailsList
//)
//{
//    TraceLoggingWrite(
//        MidiSrvAbstractionTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this")
//    );
//
//    // TODO
//    sessionDetailsList = nullptr;
//
//    return S_OK;
//
//}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::GetSessionList(
    LPWSTR* sessionList
)
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
    RETURN_HR_IF_NULL(E_INVALIDARG, sessionList);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvGetSessionList(bindingHandle.get(), sessionList));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());

    return S_OK;
}

HRESULT
CMidi2MidiSrvSessionTracker::Shutdown()
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