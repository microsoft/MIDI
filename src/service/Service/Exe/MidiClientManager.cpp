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
    m_PerformanceManager = PerformanceManager;
    m_ProcessManager = ProcessManager;
    m_DeviceManager = DeviceManager;

    return S_OK;
}

HRESULT
CMidiClientManager::Cleanup()
{
    m_PerformanceManager.reset();
    m_ProcessManager.reset();
    m_DeviceManager.reset();

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
    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;
    wil::com_ptr_nothrow<CMidiClientPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiDevicePipe> devicePipe;
    unique_mmcss_handle MmcssHandle;

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

    // Todo: determine if there is an existing midi device pipe this client can be attached to
    
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));

    // Adjust the requested buffer size to align with client requirements.
    RETURN_IF_FAILED(clientPipe->AdjustForBufferingRequirements(CreationParams));
    RETURN_IF_FAILED(devicePipe->Initialize(BindingHandle, clientProcessHandle.get(), MidiDevice, CreationParams, &m_MmcssTaskId));
    RETURN_IF_FAILED(clientPipe->Initialize(BindingHandle, clientProcessHandle.get(), MidiDevice, CreationParams, Client, &m_MmcssTaskId, devicePipe));

    // Adding the client pipe to the device pipe creates an intentional circular reference between the client
    // and device. After this point, a proper cleanup is required, simply releasing the two will, intentionally, not
    // clean up because there is a cross dependency between the client pipe and device pipe that requires a proper
    // shutdown.
    RETURN_IF_FAILED(devicePipe->AddClientPipe(clientPipe));

    // transfer this client pipe to internal tracking list
    m_ClientPipe = clientPipe;
    m_DevicePipe = devicePipe;
    Client->ClientHandle = (MidiClientHandle)m_ClientPipe.get();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientManager::DestroyMidiClient(
    handle_t /* BindingHandle */,
    MidiClientHandle ClientHandle
)
{
    // Temp, we currently only allow a single client. If this is a request
    // for that client, then destroy it. Else, invalid arg.
    //
    // Eventually, this will search for the specified pipe, confirm
    // that the caller is the owner, and remove it.
    if (ClientHandle == (MidiClientHandle)m_ClientPipe.get())
    {
        m_DevicePipe->RemoveClientPipe(m_ClientPipe);
        m_ClientPipe->Cleanup();
        m_DevicePipe->Cleanup();
        m_ClientPipe.reset();
        m_DevicePipe.reset();
    }
    else
    {
        return E_INVALIDARG;
    }

    return S_OK;
}


