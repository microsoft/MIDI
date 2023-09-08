// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

using namespace winrt::Windows::Devices::Enumeration;

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

HRESULT
GetEndpointAlias(_In_ LPCWSTR MidiDevice, _In_ std::wstring& Alias, _In_ MidiFlow& AliasFlow)
{
    Alias = MidiDevice;

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_AssociatedUMP));
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_AssociatedUMP));
    if (prop)
    {
        // this interface is pointing to a UMP interface, so use that instance id.
        Alias = winrt::unbox_value<winrt::hstring>(prop).c_str();

        // retrieve the interface class for the aliased device, either it should have the same
        // flow as requested, or it should be bidi.
        additionalProperties.Clear();
        additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));
        auto aliasedDeviceInfo = DeviceInformation::CreateFromIdAsync(Alias, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();
        prop = aliasedDeviceInfo.Properties().Lookup(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));
        auto interfaceClass = winrt::unbox_value<winrt::guid>(prop);

        if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI) == interfaceClass)
        {
            AliasFlow = MidiFlowBidirectional;
        }
        // else assume the flow is unchanged, and we will catch any inconsistencies when we attempt to
        // initialize the device pipe for the requested flow.
    }

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
    std::wstring midiDevice;
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

    MidiFlow midiFlow = CreationParams->Flow;

    // The provided SWD instance id provided may be an alias of a UMP device, retrieve the
    // PKEY_MIDI_AssociatedUMP property to retrieve the id of the primary device.
    // We only create device pipe entries for the primary devices.
    RETURN_IF_FAILED(GetEndpointAlias(MidiDevice, midiDevice, midiFlow));

    // we have either a new device or an existing device, create the client pipe and connect to it.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));

    // Adjust the requested buffer size to align with client requirements.
    RETURN_IF_FAILED(clientPipe->AdjustForBufferingRequirements(CreationParams));

    // Get an existing device pipe if one exists, otherwise create a new pipe
    auto device = m_DevicePipes.find(midiDevice.c_str());
    if (device != m_DevicePipes.end())
    {
        devicePipe = device->second;
    }
    else
    {
        // The devicePipe must be initialized with the flow of the aliased device
        MIDISRV_CLIENTCREATION_PARAMS deviceCreationParams{ *CreationParams };
        deviceCreationParams.Flow = midiFlow;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));
        RETURN_IF_FAILED(devicePipe->Initialize(BindingHandle, midiDevice.c_str(), &deviceCreationParams, &m_MmcssTaskId));
        newDeviceCreated = TRUE;
    }

    // The client pipe will add itself to the device pipe, and hold a reference to the device pipe which
    // is released when the client pipe is cleaned up.
    RETURN_IF_FAILED(clientPipe->Initialize(BindingHandle, clientProcessHandle.get(), midiDevice.c_str(), CreationParams, Client, &m_MmcssTaskId, devicePipe));

    // if a new device pipe has been created for this client, we need to transfer
    // this new reference to the device pipes list.
    if (newDeviceCreated)
    {
        // transfer this new device pipe to the device list
        m_DevicePipes.emplace(midiDevice.c_str(), std::move(devicePipe));
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

    // locate this client in the list and clean it up, which will disconnect
    // itself from the device pipe.
    // After the client is cleaned up and released, locate the device pipe
    // which was used, and if it no longer has any associated clients, clean
    // it up as well.
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


