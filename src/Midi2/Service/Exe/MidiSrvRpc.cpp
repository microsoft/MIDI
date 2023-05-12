// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t cBytes)
{
    return (void*)new (std::nothrow) BYTE[cBytes];
}

void __RPC_USER midl_user_free(void __RPC_FAR* p)
{
    delete[] (BYTE*)p;
}

// Simple test RPC to validate calling into the service and getting a service allocated
// structure returned to the client with valid contents. Simple POC of the midisrv RPC implementation.
HRESULT MidiSrvTestRpc(
    /* [in] */ handle_t /* BindingHandle */,
    /* [string][in] */ __RPC__in_string LPCWSTR Identifier,
    /* [in] */ __RPC__in PREQUESTED_SETTINGS RequestedSettings,
    /* [out] */ __RPC__deref_out_opt PALLOCATED_SETTINGS* AllocatedSettings)
{
    PALLOCATED_SETTINGS allocatedSettings = nullptr;
    GUID identifierGuid{0};

    // retrieve a shared pointer to the device manager, to confirm
    // that it can be retrieved.
    std::shared_ptr<CMidiDeviceManager> manager;

    auto cleanupOnExit = wil::scope_exit([&]() {
        if (allocatedSettings)
        {
            MIDL_user_free(allocatedSettings);
        }
    });

    RETURN_IF_FAILED(g_MidiService->GetDeviceManager(manager));

    // allocate our return structure
    allocatedSettings = (PALLOCATED_SETTINGS) MIDL_user_allocate(sizeof(ALLOCATED_SETTINGS));
    RETURN_IF_NULL_ALLOC(allocatedSettings);

    // fill in the return structure
    RETURN_IF_FAILED(CLSIDFromString(Identifier, &(allocatedSettings->Identifier)));
    allocatedSettings->Flag = RequestedSettings->Flag;
    allocatedSettings->Request = RequestedSettings->Request;
    RETURN_HR_IF(E_FAIL, RequestedSettings->Identifier != allocatedSettings->Identifier);

    // transfer the allocation
    *AllocatedSettings = allocatedSettings;
    allocatedSettings = nullptr;

    return S_OK;
}
