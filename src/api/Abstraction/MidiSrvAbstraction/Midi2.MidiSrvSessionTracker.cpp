// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


HRESULT
CMidi2MidiSrvSessionTracker::Initialize()
{
    return S_OK;
}

HRESULT
CMidi2MidiSrvSessionTracker::VerifyConnectivity()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvVerifyConnectivity(bindingHandle.get()));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

                return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::AddClientSession(
    GUID SessionId,
    LPCWSTR SessionName
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    // get process id and name
    DWORD clientProcessId = GetCurrentProcessId();

    std::wstring modulePath{ 0 };
    modulePath.resize(2048);   // MAX_PATH is almost never big enough. This is a wild shot. Not going to allocate 32k chars for this but I know this will bite me some day

    auto numPathCharacters = GetModuleFileName(NULL, modulePath.data(), (DWORD)modulePath.capacity());

    std::wstring clientProcessName{};

    if (numPathCharacters > 0)
    {
        clientProcessName = (std::filesystem::path(modulePath).filename()).c_str();
    }
    else
    {
        // couldn't get the current process name
        TraceLoggingWrite(
            MidiSrvAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to get current process name", "message")
        );
    }


    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, SessionName);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvRegisterSession(bindingHandle.get(), SessionId, SessionName, clientProcessId, clientProcessName.c_str(), &m_contextHandle));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            
            return S_OK;
        }());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::UpdateClientSessionName(
    GUID SessionId,
    LPCWSTR SessionName,
    DWORD ClientProcessId
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, SessionName);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvUpdateSessionName(bindingHandle.get(), m_contextHandle, SessionId, SessionName, ClientProcessId));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::RemoveClientSession(
    GUID SessionId
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvDeregisterSession(bindingHandle.get(), m_contextHandle, SessionId));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

            return S_OK;
        }());

    return S_OK;
}

//_Use_decl_annotations_
//HRESULT
//CMidi2MidiSrvSessionTracker::AddClientEndpointConnection(
//    GUID SessionId,
//    LPCWSTR ConnectionEndpointInterfaceId
//)
//{
//    UNREFERENCED_PARAMETER(SessionId);
//    UNREFERENCED_PARAMETER(ConnectionEndpointInterfaceId);
//
//    return S_OK;
//}
//
//_Use_decl_annotations_
//HRESULT
//CMidi2MidiSrvSessionTracker::RemoveClientEndpointConnection(
//    GUID SessionId,
//    LPCWSTR ConnectionEndpointInterfaceId
//)
//{
//    UNREFERENCED_PARAMETER(SessionId);
//    UNREFERENCED_PARAMETER(ConnectionEndpointInterfaceId);
//
//    return S_OK;
//}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::GetSessionList(
    LPSAFEARRAY* SessionDetailsList
)
{
    // TODO
    SessionDetailsList = nullptr;

    return S_OK;

}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvSessionTracker::GetSessionListJson(
    BSTR* SessionList
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, SessionList);

    //RETURN_IF_FAILED([&]()
    //    {
    //        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
    //        // with structured exception handling.
    //        RpcTryExcept RETURN_IF_FAILED(MidiSrvGetSessionList(bindingHandle.get(), SessionList));
    //        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
    //        RpcEndExcept
    //            return S_OK;
    //    }());

    return S_OK;
}

HRESULT
CMidi2MidiSrvSessionTracker::Cleanup()
{
    return S_OK;
}