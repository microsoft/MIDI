// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "Midi2ServiceTests.h"

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t cBytes)
{
    return (void*)new (std::nothrow) BYTE[cBytes];
}

void __RPC_USER midl_user_free(void __RPC_FAR* p)
{
    delete[] (BYTE*)p;
}

// using the protocol and endpoint, retrieve the midisrv
// rpc binding handle
HRESULT GetMidiSrvBindingHandle(handle_t* BindingHandle)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == BindingHandle);
    *BindingHandle = NULL;

    RPC_WSTR stringBinding = nullptr;
    auto cleanupOnExit = wil::scope_exit([&]() {
        if (stringBinding)
        {
            RpcStringFree(&stringBinding);
        }
    });

    RETURN_IF_WIN32_ERROR(RpcStringBindingCompose(
        nullptr,
        reinterpret_cast<RPC_WSTR>(MIDISRV_LRPC_PROTOCOL),
        nullptr,
        reinterpret_cast<RPC_WSTR>(MIDISRV_ENDPOINT),
        nullptr,
        &stringBinding));

    RETURN_IF_WIN32_ERROR(RpcBindingFromStringBinding(
        stringBinding,
        BindingHandle));

    return S_OK;
}

void Midi2ServiceTests::TestMidiServiceRPC()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    static const GUID identifierGuid = {0x4c4c29c3, 0xee6a, 0x4195, {0xb5, 0x56, 0xec, 0xda, 0x8f, 0x5e, 0x87, 0x2b}};
    LPCWSTR endpointIdentifier{L"{4C4C29C3-EE6A-4195-B556-ECDA8F5E872B}"};
    REQUESTED_SETTINGS requestedSettings{0};
    PALLOCATED_SETTINGS allocatedSettings{nullptr};
    wil::unique_rpc_binding bindingHandle;

    requestedSettings.Flag = TRUE;
    requestedSettings.Request = RequestEntry1;
    requestedSettings.Identifier = identifierGuid;

    auto cleanupOnExit = wil::scope_exit([&]() {
        if (allocatedSettings)
        {
            LOG_OUTPUT(L"Freeing allocated memory");
            MIDL_user_free(allocatedSettings);
        }
    });

    LOG_OUTPUT(L"Retrieving binding handle");
    VERIFY_SUCCEEDED(GetMidiSrvBindingHandle(&bindingHandle));

    VERIFY_SUCCEEDED([&]() {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvTestRpc(bindingHandle.get(), endpointIdentifier, &requestedSettings, &allocatedSettings));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    LOG_OUTPUT(L"Validating RPC call return values");
    VERIFY_IS_TRUE(allocatedSettings != nullptr);
    VERIFY_IS_TRUE(allocatedSettings->Identifier == identifierGuid);
    VERIFY_IS_TRUE(allocatedSettings->Flag == TRUE);
    VERIFY_IS_TRUE(allocatedSettings->Request == RequestEntry1);
}

bool Midi2ServiceTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    VERIFY_SUCCEEDED(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
    
    return true;
}

bool Midi2ServiceTests::ClassCleanup()
{
    Windows::Foundation::Uninitialize();
    return true;
}

