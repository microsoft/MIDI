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
CMidi2MidiSrvConfigurationManager::Initialize(
    GUID abstractionGuid, 
    IUnknown* deviceManagerInterface, 
    IUnknown* midiServiceConfigurationManagerInterface)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(deviceManagerInterface);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    m_abstractionGuid = abstractionGuid;

    return S_OK;

}





_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::UpdateConfiguration(LPCWSTR configurationJson, BOOL IsFromConfigurationFile, BSTR* response)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
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
            RpcTryExcept RETURN_IF_FAILED(MidiSrvUpdateConfiguration(bindingHandle.get(), configurationJson, IsFromConfigurationFile, response));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetAbstractionList(BSTR* AbstractionListJson)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, AbstractionListJson);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvGetAbstractionList(bindingHandle.get(), AbstractionListJson));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
            return S_OK;
        }());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::GetTransformList(BSTR* TransformListJson)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));
    RETURN_HR_IF_NULL(E_INVALIDARG, TransformListJson);

    RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvGetTransformList(bindingHandle.get(), TransformListJson));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
                return S_OK;
        }());

    return S_OK;
}















HRESULT
CMidi2MidiSrvConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    return S_OK;
}

