// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiClientManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& PerformanceManager,
    std::shared_ptr<CMidiProcessManager>& ProcessManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager
)
{
    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager = PerformanceManager;
    m_ProcessManager = ProcessManager;
    m_DeviceManager = DeviceManager;

    return S_OK;
}

HRESULT
CMidiClientManager::Cleanup()
{
    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager.reset();
    m_ProcessManager.reset();
    m_DeviceManager.reset();

    for (auto const& Client : m_ClientPipes)
    {
        Client.second->Cleanup();
    }
    m_ClientPipes.clear();

    for (auto const& Device : m_DevicePipes)
    {
        Device.second->Cleanup();
    }
    m_DevicePipes.clear();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientManager::CreateMidiClient(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client
)
{
    auto lock = m_ClientManagerLock.lock();

    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;
    wil::com_ptr_nothrow<CMidiClientPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiDevicePipe> devicePipe;
    unique_mmcss_handle MmcssHandle;
    BOOL newDeviceCreated{ FALSE };

    // get the client PID, impersonate the client to get the client process handle, and then
    // revert back to self.
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(I_RpcBindingInqLocalClientPID(BindingHandle, &clientProcessId)));
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcImpersonateClient(BindingHandle)));
    clientProcessHandle.reset(OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, clientProcessId));
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcRevertToSelf()));

    RETURN_LAST_ERROR_IF_NULL(clientProcessHandle);

    // pre-populate our mmcss task id so we have a known task id
    // to provide and use for all other tasks related to this pipe
    // retain the MMCSS configuration until after the client and device
    // pipes are initialized, so this TaskId remains valid through
    // initialization. Safe to disable mmcss for this rpc thread,
    // once we complete initialization and have worker threads
    // actively using this task id for the duration of this pipe.
    RETURN_IF_FAILED(EnableMmcss(MmcssHandle, m_MmcssTaskId));

    // check to see if there is an existing device pipe.
    // TODO: if this is a legacy instance id, retrieve the mapped UMP instance
    // id if one exists, if not use the legacy instance id.
    // TODO: anything required for the client creation params?
    auto device = m_DevicePipes.find(MidiDevice);
    if (device != m_DevicePipes.end())
    {
        devicePipe = device->second;
    }
    else
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));
        RETURN_IF_FAILED(devicePipe->Initialize(BindingHandle, MidiDevice, CreationParams, &m_MmcssTaskId));
        newDeviceCreated = TRUE;
    }

    // we have either a new device or an existing device, create the client pipe and connect to it.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));

    // Adjust the requested buffer size to align with client requirements.
    RETURN_IF_FAILED(clientPipe->AdjustForBufferingRequirements(CreationParams));
    RETURN_IF_FAILED(clientPipe->Initialize(BindingHandle, clientProcessHandle.get(), MidiDevice, CreationParams, Client, &m_MmcssTaskId, devicePipe));

    if (newDeviceCreated)
    {
        // transfer this new device pipe to the device list
        m_DevicePipes.emplace(MidiDevice, std::move(devicePipe));
    }

    // transfer this new client pipe to internal tracking list
    Client->ClientHandle = (MidiClientHandle)clientPipe.get();
    m_ClientPipes.emplace(Client->ClientHandle, std::move(clientPipe));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientManager::DestroyMidiClient(
    handle_t /* BindingHandle */,
    MidiClientHandle ClientHandle
)
{
    auto lock = m_ClientManagerLock.lock();

    // Temp, we currently only allow a single client. If this is a request
    // for that client, then destroy it. Else, invalid arg.
    //
    // Eventually, this will search for the specified pipe, confirm
    // that the caller is the owner, and remove it.
    auto client = m_ClientPipes.find(ClientHandle);
    if (client != m_ClientPipes.end())
    {
        std::wstring deviceInstanceId = client->second->MidiDevice();

        client->second->Cleanup();
        m_ClientPipes.erase(client);

        // if this was the last client using this device pipe, clean
        // up the device pipe as well.
        auto device = m_DevicePipes.find(deviceInstanceId);

        if (device != m_DevicePipes.end())
        {
            if (!device->second->InUse())
            {
                device->second->Cleanup();
                m_DevicePipes.erase(device);
            }
        }
    }
    else
    {
        return E_INVALIDARG;
    }

    return S_OK;
}


