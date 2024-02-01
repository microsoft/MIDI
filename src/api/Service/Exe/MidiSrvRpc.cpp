// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"



_Use_decl_annotations_
void *midl_user_allocate(size_t size)
{
    return (void*)new (std::nothrow) BYTE[size];
}

_Use_decl_annotations_
void midl_user_free(void* p)
{
    delete[] (BYTE*)p;
}

// To be removed
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

HRESULT MidiSrvCreateClient(
    /* [in] */ handle_t BindingHandle,
    /* [string][in] */ __RPC__in_string LPCWSTR MidiDevice,
    /* [in] */ __RPC__in PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    /* [out] */ __RPC__deref_out_opt PMIDISRV_CLIENT *Client)
{
    std::shared_ptr<CMidiClientManager> clientManager;
    PMIDISRV_CLIENT createdClient {nullptr};

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    auto cleanupOnExit = wil::scope_exit([&]() {
        if (createdClient)
        {
            MIDL_user_free(createdClient);
        }
    });

    // allocate a midl object to fill in and return to the client
    createdClient = (PMIDISRV_CLIENT) MIDL_user_allocate(sizeof(MIDISRV_CLIENT));
    RETURN_IF_NULL_ALLOC(createdClient);

    ZeroMemory(createdClient, sizeof(MIDISRV_CLIENT));

    // Client manager creates the client, fills in the MIDISRV_CLIENT information
    RETURN_IF_FAILED(g_MidiService->GetClientManager(clientManager));
    RETURN_IF_FAILED(clientManager->CreateMidiClient(BindingHandle, MidiDevice, CreationParams, createdClient));

    // Success, transfer the MIDISRV_CLIENT data to the caller.
    *Client = createdClient;
    createdClient = nullptr;

    return S_OK;
}

HRESULT MidiSrvDestroyClient(
    /* [in] */ handle_t BindingHandle,
    /* [in] */ __RPC__in MidiClientHandle ClientHandle)
{
    std::shared_ptr<CMidiClientManager> clientManager;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // Client manager creates the client, fills in the MIDISRV_CLIENT information
    RETURN_IF_FAILED(g_MidiService->GetClientManager(clientManager));
    RETURN_IF_FAILED(clientManager->DestroyMidiClient(BindingHandle, ClientHandle));

    return S_OK;
}


HRESULT MidiSrvUpdateConfiguration(
    /* [in] */ handle_t BindingHandle,
    /*[in, string]*/ __RPC__in_string LPCWSTR ConfigurationJson)
{
    UNREFERENCED_PARAMETER(BindingHandle);
    UNREFERENCED_PARAMETER(ConfigurationJson);


    // Send it to the configuration manager and get it broken apart and sent to
    // all the destinations it needs to get to (transports, transforms, etc.)






    return S_OK;
}


