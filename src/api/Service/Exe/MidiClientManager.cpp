// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

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

    for (auto const& client : m_ClientPipes)
    {
        client.second->Cleanup();
    }
    m_ClientPipes.clear();

    for (auto const& device : m_DevicePipes)
    {
        device.second->Cleanup();
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
        RETURN_HR_IF_NULL(E_INVALIDARG, prop);
        auto interfaceClass = winrt::unbox_value<winrt::guid>(prop);

        if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI) == interfaceClass)
        {
            AliasFlow = MidiFlowBidirectional;
        }
        // else assume the flow is unchanged, and we will catch any inconsistencies when we attempt to
        // initialize the device pipe for the requested flow.
    }

    std::transform(Alias.begin(), Alias.end(), Alias.begin(), ::towlower);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiClient(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client,
    wil::unique_handle& ClientProcessHandle,
    wil::com_ptr_nothrow<CMidiPipe>&MidiClientPipe
)
{
    wil::com_ptr_nothrow<CMidiClientPipe> clientPipe;

    // we have either a new device or an existing device, create the client pipe and connect to it.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));

    // Initialize our audio client
    RETURN_IF_FAILED(clientPipe->Initialize(BindingHandle, ClientProcessHandle.get(), MidiDevice, CreationParams, Client, &m_MmcssTaskId));

    // Add this client to the client pipes list and set the output client handle
    Client->ClientHandle = (MidiClientHandle)clientPipe.get();
    MidiClientPipe = clientPipe.get();
    m_ClientPipes.emplace(Client->ClientHandle, std::move(clientPipe));

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiDevice(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    wil::com_ptr_nothrow<CMidiPipe>& MidiDevicePipe
)
{
    // Get an existing device pipe if one exists, otherwise create a new pipe
    auto device = m_DevicePipes.find(MidiDevice);
    if (device != m_DevicePipes.end())
    {
        RETURN_HR_IF(E_UNEXPECTED, device->second->Flow() != CreationParams->Flow);
        MidiDevicePipe = device->second;
    }
    else
    {
        wil::com_ptr_nothrow<CMidiDevicePipe> devicePipe;

        // The devicePipe must be initialized with the flow of the aliased device
        MIDISRV_DEVICECREATION_PARAMS deviceCreationParams{ 0 };

        deviceCreationParams.BufferSize = CreationParams->BufferSize;
        // create the device using the devices preferred format.
        // if that mismatches the client capabilities, a transform will be inserted.
        deviceCreationParams.DataFormat = MidiDataFormat_Any;
        deviceCreationParams.Flow = CreationParams->Flow;

        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));
        RETURN_IF_FAILED(devicePipe->Initialize(BindingHandle, MidiDevice, &deviceCreationParams, &m_MmcssTaskId));

        MidiDevicePipe = devicePipe.get();
        m_DevicePipes.emplace(MidiDevice, std::move(devicePipe));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiTransform(
    handle_t BindingHandle,
    MidiFlow Flow,
    MidiDataFormat DataFormatFrom,
    MidiDataFormat DataFormatTo,
    wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe
)
{
    wil::com_ptr_nothrow<CMidiPipe> transformPipe;

    // no transform is required, this shouldn't have been called.
    RETURN_HR_IF(E_UNEXPECTED, DataFormatFrom == DataFormatTo);

    // search existing transforms for this device for an output that supports
    // the requested flow and data format.
    auto transforms = m_TransformPipes.equal_range(DevicePipe->MidiDevice());
    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
    {
        if (Flow == transform->second->Flow() &&
            DataFormatFrom == transform->second->DataFormatIn() &&
            DataFormatTo == transform->second->DataFormatOut())
        {
            RETURN_HR_IF(E_UNEXPECTED, transformPipe);
            transformPipe = transform->second;
        }
    }

    // not found, instantiate the transform that is needed.
    if (!transformPipe)
    {
        MIDISRV_TRANSFORMCREATION_PARAMS creationParams {0};

        creationParams.Flow = Flow;
        creationParams.DataFormatIn = DataFormatFrom;
        creationParams.DataFormatOut = DataFormatTo;

        if (MidiDataFormat_UMP == DataFormatFrom &&
            MidiDataFormat_ByteStream == DataFormatTo)
        {
            creationParams.TransformGuid = __uuidof(Midi2UMP2BSTransform);
        }
        else if (MidiDataFormat_ByteStream == DataFormatFrom &&
            MidiDataFormat_UMP == DataFormatTo)
        {
            creationParams.TransformGuid = __uuidof(Midi2BS2UMPTransform);
        }
        else
        {
            // unknown transform
            RETURN_IF_FAILED(ERROR_UNSUPPORTED_TYPE);
        }

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));
        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId));
        transformPipe = transform.get();

        // connect the transform to the device
        if (Flow == MidiFlowIn)
        {
            RETURN_IF_FAILED(DevicePipe->AddConnectedPipe(transformPipe));
        }
        else
        {
            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(DevicePipe));
        }

        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transformPipe);
    }

    ClientConnectionPipe = transformPipe;
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
    wil::com_ptr_nothrow<CMidiPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiPipe> devicePipe;
    wil::com_ptr_nothrow<CMidiPipe> clientConnectionPipe;

    unique_mmcss_handle MmcssHandle;
    std::wstring midiDevice;

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

    // The provided SWD instance id provided may be an alias of a UMP device, retrieve the
    // PKEY_MIDI_AssociatedUMP property to retrieve the id of the primary device.
    // We only create device pipe entries for the primary devices.
    RETURN_IF_FAILED(GetEndpointAlias(MidiDevice, midiDevice, CreationParams->Flow));
    RETURN_IF_FAILED(GetMidiClient(BindingHandle, midiDevice.c_str(), CreationParams, Client, clientProcessHandle, clientPipe));

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        // If a new device has been created and added to m_DevicePipes,
        // removing the client will also remove the unused device
        DestroyMidiClient(BindingHandle, Client->ClientHandle);
        Client->ClientHandle = NULL;
        Client->DataFormat = MidiDataFormat_Invalid;
    });

    RETURN_IF_FAILED(GetMidiDevice(BindingHandle, midiDevice.c_str(), CreationParams, devicePipe));
    devicePipe->AddClient((MidiClientHandle)clientPipe.get());

    // MidiFlowIn on the client flows data from the midi device to the client,
    // so we register the clientPipe to receive the callbacks from the clientConnectionPipe.
    if (clientPipe->IsFlowSupported(MidiFlowIn))
    {
        // If the client supports the same format that the device pipe supports,
        // or if the client supports any format, then connect directly to
        // the device.
        if (clientPipe->IsFormatSupportedIn(devicePipe->DataFormatIn()))
        {
            clientConnectionPipe = devicePipe;
        }
        else
        {
            // client requires a specific format, retrieve the transform required for that format.
            RETURN_IF_FAILED(GetMidiTransform(BindingHandle, MidiFlowIn, devicePipe->DataFormatIn(), clientPipe->DataFormatIn(), devicePipe, clientConnectionPipe));
            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
        }

        RETURN_IF_FAILED(clientPipe->SetDataFormatIn(clientConnectionPipe->DataFormatIn()));
        Client->DataFormat = clientPipe->DataFormatIn();
        RETURN_IF_FAILED(clientConnectionPipe->AddConnectedPipe(clientPipe));
        clientConnectionPipe.reset();
    }

    // MidiFlowOut on the client flows data from the midi client to the device,
    // so we register the clientConnectionPipe to receive the callbacks from the clientPipe.
    if (clientPipe->IsFlowSupported(MidiFlowOut))
    {
        // If the client supports the same format that the device pipe supports,
        // or if the client supports any format, then connect directly to
        // the device.
        if (clientPipe->IsFormatSupportedOut(devicePipe->DataFormatOut()))
        {
            clientConnectionPipe = devicePipe;
        }
        else
        {
            // client requires a specific format, retrieve the transform required for that format.
            RETURN_IF_FAILED(GetMidiTransform(BindingHandle, MidiFlowOut, clientPipe->DataFormatOut(), devicePipe->DataFormatOut(), devicePipe, clientConnectionPipe));
            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
        }

        RETURN_IF_FAILED(clientPipe->SetDataFormatOut(clientConnectionPipe->DataFormatOut()));
        Client->DataFormat = clientPipe->DataFormatOut();
        RETURN_IF_FAILED(clientPipe->AddConnectedPipe(clientConnectionPipe));
        clientConnectionPipe.reset();
    }

    cleanupOnFailure.release();

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
        wil::com_ptr_nothrow<CMidiPipe> midiClientPipe = client->second.get();

        midiClientPipe->Cleanup();

        for (auto transform = m_TransformPipes.begin(); transform != m_TransformPipes.end();)
        {
            wil::com_ptr_nothrow<CMidiPipe> midiTransformPipe = transform->second.get();
            midiTransformPipe->RemoveClient(ClientHandle);
            if (!midiTransformPipe->InUse())
            {
                midiTransformPipe->Cleanup();
                transform = m_TransformPipes.erase(transform);
            }
            else
            {
                transform++;
            }
        }

        for (auto device = m_DevicePipes.begin(); device != m_DevicePipes.end();)
        {
            wil::com_ptr_nothrow<CMidiPipe> midiDevicePipe = device->second.get();
            midiDevicePipe->RemoveClient(ClientHandle);
            if (!midiDevicePipe->InUse())
            {
                midiDevicePipe->Cleanup();
                device = m_DevicePipes.erase(device);
            }
            else
            {
                device++;
            }
        }

        m_ClientPipes.erase(client);
    }
    else
    {
        return E_INVALIDARG;
    }

    return S_OK;
}


