// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#pragma warning(push)
#pragma warning(disable: 6031)     // ignored return value in RPC code

#include "MidiSrvRpc_c.c"   // not for use directly by applications. The RPC calls are an internal implementation detail and may change at any point
#include "midicheckservice_rpc.h"

// this RPC code is used by this tool, but is not supported for use by applications 
// directly. It could change in the future, as an internal implementation detail of 
// Windows MIDI Services.

_Use_decl_annotations_
void __RPC_FAR* __RPC_USER midl_user_allocate(size_t byteCount
)
{
    return (void*)new (std::nothrow) BYTE[byteCount];
}

_Use_decl_annotations_
void __RPC_USER midl_user_free(void __RPC_FAR* pointer
)
{
    delete[](BYTE*)pointer;
}

bool VerifyMidiSrvConnectivity()
{
    wil::unique_rpc_binding bindingHandle;
    bindingHandle = NULL;

    RPC_WSTR stringBinding = nullptr;
    auto cleanupOnExit = wil::scope_exit([&]()
        {
            if (stringBinding)
            {
                RpcStringFree(&stringBinding);
            }
        });

    RpcStringBindingCompose(
        nullptr,
        (RPC_WSTR)MIDISRV_LRPC_PROTOCOL,
        nullptr,
        nullptr,
        nullptr,
        &stringBinding);

    RpcBindingFromStringBinding(
        stringBinding,
        &bindingHandle);

    bool result = false;

    auto verifyConnectivity = ([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept result = MidiSrvVerifyConnectivity(bindingHandle.get());
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept

                return S_OK;
        });

    auto hr = verifyConnectivity();


    if (FAILED(hr))
    {
        return false;
    }

    return result;
}


#pragma warning(pop)

// End RPC code ---------------



