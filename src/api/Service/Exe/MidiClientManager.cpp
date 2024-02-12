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
    std::shared_ptr<CMidiDeviceManager>& DeviceManager,
    std::shared_ptr<CMidiSessionTracker>& SessionTracker
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager = PerformanceManager;
    m_ProcessManager = ProcessManager;
    m_DeviceManager = DeviceManager;
    m_SessionTracker = SessionTracker;

    return S_OK;
}

HRESULT
CMidiClientManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    OutputDebugString(L"" __FUNCTION__ " enter");
    
    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager.reset();
    m_ProcessManager.reset();
    m_DeviceManager.reset();
    m_SessionTracker.reset();

    // tear down all transforms before we clean up the client/device
    for (auto const& transform : m_TransformPipes)
    {
        transform.second->Cleanup();
    }
    m_TransformPipes.clear();

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

    OutputDebugString(L"" __FUNCTION__ " exit");

    return S_OK;
}


HRESULT
GetEndpointShouldHaveMetadataHandler(_In_ std::wstring MidiDevice, _Inout_ bool& AddMetadataListener, _In_ MidiFlow& Flow)
{
    if (Flow == MidiFlow::MidiFlowBidirectional || Flow == MidiFlow::MidiFlowIn)
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler));
        auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler));
        if (prop)
        {
            OutputDebugString(__FUNCTION__ L" found property");

            // this interface is pointing to a UMP interface, so use that instance id.
            AddMetadataListener = winrt::unbox_value<bool>(prop);
        }
        else
        {
            OutputDebugString(__FUNCTION__ L" did not find property");

            // default to true
            AddMetadataListener = true;
        }
    }
    else
    {
        // output-only flow
        AddMetadataListener = false;
    }

    return S_OK;
}


HRESULT
GetDeviceSupportedDataFormat(_In_ std::wstring MidiDevice, _Inout_ MidiDataFormat& DataFormat)
{
    OutputDebugString(__FUNCTION__ L" enter");

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
    if (prop)
    {

        OutputDebugString(__FUNCTION__ L" found property");

        DataFormat = MidiDataFormat::MidiDataFormat_Any;
        try
        {
            // this interface is pointing to a UMP interface, so use that instance id.
            DataFormat = (MidiDataFormat)winrt::unbox_value<UINT32>(prop);
        }
        CATCH_LOG();
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" didn't find property");
        // default to any
        DataFormat = MidiDataFormat::MidiDataFormat_Any;
    }

    OutputDebugString(__FUNCTION__ L" exiting OK");

    return S_OK;
}

HRESULT
GetEndpointGenerateIncomingTimestamp(_In_ std::wstring MidiDevice, _Inout_ bool& GenerateIncomingTimestamp, _In_ MidiFlow& Flow)
{
    // we only generate timestamps for incoming messages FROM the device
    if (Flow == MidiFlow::MidiFlowBidirectional || Flow == MidiFlow::MidiFlowIn)
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_GenerateIncomingTimestamp));
        auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_GenerateIncomingTimestamp));
        if (prop)
        {
            // this interface is pointing to a UMP interface, so use that instance id.
            GenerateIncomingTimestamp = winrt::unbox_value<bool>(prop);
        }
        else
        {
            // default to true
            GenerateIncomingTimestamp = true;
        }
    }
    else
    {
        // output-only flow
        GenerateIncomingTimestamp = false;
    }

    return S_OK;
}

HRESULT
GetEndpointAlias(_In_ LPCWSTR MidiDevice, _In_ std::wstring& Alias, _In_ MidiFlow& AliasFlow)
{
    OutputDebugString(__FUNCTION__ L" enter. Device:");
    OutputDebugString(MidiDevice);

    Alias = MidiDevice;

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_AssociatedUMP));
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    OutputDebugString(__FUNCTION__ L" looking up prop");
    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_AssociatedUMP));

    OutputDebugString(__FUNCTION__ L" got prop. About to check for null");

    if (prop)
    {
        OutputDebugString(L"" __FUNCTION__ " STRING_PKEY_MIDI_AssociatedUMP property present");
        
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

    OutputDebugString(__FUNCTION__ L" exit. Alias:");
    OutputDebugString(Alias.c_str());

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiClient(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    GUID SessionId,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client,
    wil::unique_handle& ClientProcessHandle,
    wil::com_ptr_nothrow<CMidiPipe>&MidiClientPipe,
    BOOL OverwriteIncomingZeroTimestamps
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice),
        TraceLoggingGuid(SessionId)
    );

    wil::com_ptr_nothrow<CMidiClientPipe> clientPipe;

    // we have either a new device or an existing device, create the client pipe and connect to it.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));

    // Initialize our client
    RETURN_IF_FAILED(clientPipe->Initialize(BindingHandle, ClientProcessHandle.get(), MidiDevice, SessionId, CreationParams, Client, &m_MmcssTaskId, OverwriteIncomingZeroTimestamps));

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
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice)
    );

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
        MIDISRV_DEVICECREATION_PARAMS deviceCreationParams{ };

        deviceCreationParams.BufferSize = CreationParams->BufferSize;

        // create the device using the device's preferred format.
        // if that mismatches the client capabilities, a transform will be inserted.
        MidiDataFormat dataFormat = MidiDataFormat::MidiDataFormat_Any;
        LOG_IF_FAILED(GetDeviceSupportedDataFormat(MidiDevice, dataFormat));

        deviceCreationParams.DataFormat = dataFormat;
        deviceCreationParams.Flow = CreationParams->Flow;

        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));
        RETURN_IF_FAILED(devicePipe->Initialize(BindingHandle, MidiDevice, &deviceCreationParams, &m_MmcssTaskId));

        MidiDevicePipe = devicePipe.get();
        m_DevicePipes.emplace(MidiDevice, std::move(devicePipe));
    }

    return S_OK;
}

// This function handles data format translation only
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
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

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
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Adding Midi2UMP2BSTransform (UMP to Bytestream)")
            );

            creationParams.TransformGuid = __uuidof(Midi2UMP2BSTransform);
        }
        else if (MidiDataFormat_ByteStream == DataFormatFrom &&
            MidiDataFormat_UMP == DataFormatTo)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Adding Midi2BS2UMPTransform (Bytestream to UMP)")
            );

            creationParams.TransformGuid = __uuidof(Midi2BS2UMPTransform);
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unsupported translation type requested. This is probably an error in specifying data format.")
            );

            // unknown transform
            RETURN_IF_FAILED(ERROR_UNSUPPORTED_TYPE);
        }

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));
        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, (IUnknown*)&m_DeviceManager));
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

        //m_TransformPipes.emplace(DevicePipe->MidiDevice(), transformPipe);
        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transform);
    }

    ClientConnectionPipe = transformPipe;

    return S_OK;
}


// This isn't a generic plugin "get" function because scheduling is a system-managed feature, 
// like data format translation and JR Timestamp management. Also, like those, only one can be
// associated with any given connection / device. 
_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiScheduler(
    handle_t BindingHandle,
    MidiFlow Flow,
    wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& NextDeviceSidePipe,
    wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

    // we only schedule outgoing messages
    RETURN_HR_IF(E_UNEXPECTED, Flow != MidiFlow::MidiFlowOut);

    // Order of some transforms is important:
    // MIDI 1/2 New ACX Device: [All other transforms]->[Scheduler]->[JR Timestamps]->[UMP Device]
    // MIDI 1 Legacy KS Device: [All other transforms]->[Scheduler]->[Data Format Translation]->[Byte Stream Device]

    // As long as the format is correct, we always add the scheduler, but a timestamp of 0 will bypass it.
    // This will allow us to potentially add scheduling to legacy APIs if there was already planned support
    // there. Not a v1 thing due to potential compat behavior issues. We may decide not to do this at all.

    // See if we already have a scheduler attached for this device
    // if so, return that, because we only want a single scheduler per device
    auto transforms = m_TransformPipes.equal_range(DevicePipe->MidiDevice());
    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
    {
        //wil::com_ptr_nothrow<CMidiTransformPipe> pipe = transform->second.get();

        if (transform->second->TransformGuid() == __uuidof(Midi2SchedulerTransform))
        {
            transformPipe = transform->second;
            break;
        }
    }

    // not found, instantiate the transform that is needed.
    if (!transformPipe)
    {
//        OutputDebugString(L"" __FUNCTION__ " scheduler transform pipe not found. Creating one.");

        MIDISRV_TRANSFORMCREATION_PARAMS creationParams{ 0 };

        creationParams.Flow = Flow;
        creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_UMP;
        creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;

        creationParams.TransformGuid = __uuidof(Midi2SchedulerTransform);

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, (IUnknown*) & m_DeviceManager));

        transformPipe = transform.get();

        //// connect the transform to the device
        //if (Flow == MidiFlowIn)
        //{
        //    RETURN_IF_FAILED(NextDeviceSidePipe->AddConnectedPipe(transformPipe));
        //}
        //else
        //{
            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(NextDeviceSidePipe));
        //}

        //m_TransformPipes.emplace(DevicePipe->MidiDevice(), transformPipe);
        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transform);
    }

    ClientConnectionPipe = transformPipe;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiEndpointMetadataHandler(
    handle_t BindingHandle,
    MidiFlow Flow,
    wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& NextDeviceSidePipe,
    wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

    // we only handle metadata on incoming messages
    RETURN_HR_IF(E_UNEXPECTED, Flow != MidiFlow::MidiFlowIn);


    auto transforms = m_TransformPipes.equal_range(DevicePipe->MidiDevice());
    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
    {
        //wil::com_ptr_nothrow<CMidiTransformPipe> pipe = transform->second.get();

        if (transform->second->TransformGuid() == __uuidof(Midi2EndpointMetadataListenerTransform))
        {
            transformPipe = transform->second;
            break;
        }
    }

    // not found, instantiate the transform that is needed.
    if (!transformPipe)
    {
        MIDISRV_TRANSFORMCREATION_PARAMS creationParams{ 0 };

        creationParams.Flow = Flow;
        creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_UMP;
        creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;

        creationParams.TransformGuid = __uuidof(Midi2EndpointMetadataListenerTransform);

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, (IUnknown*)m_DeviceManager.get()));

        transformPipe = transform.get();

        // connect the transform to the device

        if (Flow == MidiFlowIn)
        {
            RETURN_IF_FAILED(NextDeviceSidePipe->AddConnectedPipe(transformPipe));
        }
        else
        {
            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(NextDeviceSidePipe));
        }

        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transform);
    }

    ClientConnectionPipe = transformPipe;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiJRTimestampHandler(
    handle_t BindingHandle,
    MidiFlow Flow,
    wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& NextDeviceSidePipe,
    wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(BindingHandle);
    UNREFERENCED_PARAMETER(Flow);
    UNREFERENCED_PARAMETER(DevicePipe);
    UNREFERENCED_PARAMETER(NextDeviceSidePipe);
    UNREFERENCED_PARAMETER(ClientConnectionPipe);



    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiClientManager::CreateMidiClient(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    GUID SessionId,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice),
        TraceLoggingGuid(SessionId)
    );

    //switch (CreationParams->DataFormat)
    //{
    //case MidiDataFormat::MidiDataFormat_Any:
    //    OutputDebugString(__FUNCTION__ L" CreationParams->DataFormat = MidiDataFormat_Any\n");
    //    break;
    //case MidiDataFormat::MidiDataFormat_ByteStream:
    //    OutputDebugString(__FUNCTION__ L" CreationParams->DataFormat = MidiDataFormat_ByteStream\n");
    //    break;
    //case MidiDataFormat::MidiDataFormat_UMP:
    //    OutputDebugString(__FUNCTION__ L" CreationParams->DataFormat = MidiDataFormat_UMP\n");
    //    break;
    //case MidiDataFormat::MidiDataFormat_Invalid:
    //    OutputDebugString(__FUNCTION__ L" CreationParams->DataFormat = MidiDataFormat_Invalid\n");
    //    break;
    //default:
    //    OutputDebugString(__FUNCTION__ L" CreationParams->DataFormat = some other invalid value\n");
    //    break;
    //}

    auto lock = m_ClientManagerLock.lock();

    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;
    wil::com_ptr_nothrow<CMidiPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiPipe> devicePipe;
    wil::com_ptr_nothrow<CMidiPipe> clientConnectionPipe;

    wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe{ nullptr };

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

    // see if we should create timestamps or not. This option is set
    // by the transport at endpoint enumeration time. For most endpoint
    // types, this will be true, but for diagnostics loopback endpoints, 
    // it is false. So, we store the value in the property store to make
    // it more flexible
    bool generateIncomingMessageTimestamps{ true };
    RETURN_IF_FAILED(GetEndpointGenerateIncomingTimestamp(midiDevice, generateIncomingMessageTimestamps, CreationParams->Flow));

    // Some endpoints, like the device-side of an app-to-app MIDI connection, 
    // should not have metadata listeners. This flag controls whether or not
    // we add one if otherwise eligible. 
    bool addMetadataListenerToIncomingStream{ true };
    RETURN_IF_FAILED(GetEndpointShouldHaveMetadataHandler(midiDevice, addMetadataListenerToIncomingStream, CreationParams->Flow));
    

    RETURN_IF_FAILED(GetMidiClient(BindingHandle, midiDevice.c_str(), SessionId, CreationParams, Client, clientProcessHandle, clientPipe, generateIncomingMessageTimestamps));

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
        OutputDebugString(L"" __FUNCTION__ " Checking for MidiFlowIn transforms required");

        // If the client supports the same format that the device pipe supports,
        // or if the client supports any format, then connect directly to
        // the device.
        if (clientPipe->IsFormatSupportedIn(devicePipe->DataFormatIn()))
        {
            OutputDebugString(L"" __FUNCTION__ " No MidiFlowIn format translation required");

            clientConnectionPipe = devicePipe;
        }
        else
        {
            OutputDebugString(L"" __FUNCTION__ " Adding MidiFlowIn data format translator");

            // client requires a specific format, retrieve the transform required for that format.
            RETURN_IF_FAILED(GetMidiTransform(
                BindingHandle, 
                MidiFlowIn, 
                devicePipe->DataFormatIn(), 
                clientPipe->DataFormatIn(), 
                devicePipe, 
                clientConnectionPipe)); // clientConnectionPipe is the plugin

            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
        }
        newClientConnectionPipe = clientConnectionPipe;


        // Metadata Listener ----------------------------------------------------------------
        // We should check protocol, not just data format here because we're putting this on
        // MIDI 1.0 devices that use the new driver, and there's no reason to do that.
        if (addMetadataListenerToIncomingStream && devicePipe->IsFormatSupportedIn(MidiDataFormat::MidiDataFormat_UMP))
        {
            OutputDebugString(L"" __FUNCTION__ " Adding MidiFlowIn metadata handler");

            // Our clientConnectionPipe is now the Scheduler
            RETURN_IF_FAILED(GetMidiEndpointMetadataHandler(
                BindingHandle,
                MidiFlowIn,
                devicePipe,
                newClientConnectionPipe,
                clientConnectionPipe)); // clientConnectionPipe is the plugin

            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
        }
        newClientConnectionPipe = clientConnectionPipe;




        // no more transforms to add
        clientConnectionPipe = newClientConnectionPipe;


        RETURN_IF_FAILED(clientPipe->SetDataFormatIn(clientConnectionPipe->DataFormatIn()));
        Client->DataFormat = clientPipe->DataFormatIn();
        RETURN_IF_FAILED(clientConnectionPipe->AddConnectedPipe(clientPipe));
        clientConnectionPipe.reset();
    }

    // MidiFlowOut on the client flows data from the midi client to the device,
    // so we register the clientConnectionPipe to receive the callbacks from the clientPipe.
    if (clientPipe->IsFlowSupported(MidiFlowOut))
    {
        OutputDebugString(L"" __FUNCTION__ " Creating MidiFlowOut transforms");

        // TODO: Need to see if there's a translator or JR Timestamp provider at the end of this
        // and if so, connect to that, not to the device pipe itself.
        //
        // Maybe redo the flow so:
        //
        //     Set the device as the current end pipe   
        // 
        //     Read the device properties to decide what infrastructure plugins we need (MIDI 1.0
        //     translators needed for bytestream, JR timestamps and metadata listeners only for
        //     MIDI 2.0 protocol devices, etc.)
        // 
        //     Then:
        // 
        //     - Do we need translation from UMP to bytestream? If so, stick that before the device 
        //       pipe and then use that as our current end pipe
        //
        //     - Do we need JR Timestamps? (Yes for anything that is MIDI 2.0, but disabled by 
        //       default) If so, put that before the end pipe and also add to the input side, 
        //       linking the two, maybe by providing the same instance GUID so the plugin's manager
        //       can do the association internally and ensure they have the same clock, device, etc.
        //
        //     - Add the message scheduler before the current end pipe. Message scheduler is always
        //       added regardless of protocol in use.
        //
        //     - Add metadata listeners to the input pipes as well for bidirectional MIDI 2.0 
        //       endpoints
        //
        //     - Finally, add any requested input or output processing plugins. These will come
        //       in the form of two lists of GUIDs with some configuration information, so maybe
        //       not a list of GUIDs, but a pile of JSON with GUID + properties for each.


        // our initial state is straight through to the device pipe
        clientConnectionPipe = devicePipe;
        newClientConnectionPipe = devicePipe;

        // JR Timestamps ---------------------------------------------------
        // TODO: these are only for MIDI 2.0 clients, so need to do additional property checking here
        // TODO: There should be only one JR Timestamp handler in each direction connected to the device
        // TODO: The data format is not sufficient to know if it is UMP. We need to check the native format
        //if (devicePipe->DataFormatOut() == MidiDataFormat_UMP)
        //{
        //    // Add JR timestamp 
        //    RETURN_IF_FAILED(GetMidiJRTimestampHandler(BindingHandle, MidiFlowOut, devicePipe, newClientConnectionPipe, clientConnectionPipe));

        //    // Our clientConnectionPipe is now the JR Timestamp generator
        //    newClientConnectionPipe = clientConnectionPipe;
        //}


        // TODO: This needs to work with both bytestream and UMP clients, so this logic needs to change

        // Data Format Translator ---------------------------------------------------
        if (!clientPipe->IsFormatSupportedOut(newClientConnectionPipe->DataFormatOut()))
        {
            OutputDebugString(L"" __FUNCTION__ " Adding MidiFlowOut format translator");

            // Format is not supported, so we need to transform
            // client requires a specific format, retrieve the transform required for that format.
            // Our clientConnectionPipe is now the format translator

            RETURN_IF_FAILED(GetMidiTransform(
                BindingHandle, 
                MidiFlowOut, 
                clientPipe->DataFormatOut(), 
                newClientConnectionPipe->DataFormatOut(), 
                devicePipe, 
                clientConnectionPipe)); // clientConnectionPipe is the plugin

            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());

            newClientConnectionPipe = clientConnectionPipe;
        }

        // Scheduler ----------------------------------------------------------------
        // for now, the scheduler is only going to work with UMP, so we hope
        // any required translation is done BEFORE we add this.
        if (clientConnectionPipe->IsFormatSupportedOut(MidiDataFormat::MidiDataFormat_UMP))
        {
            OutputDebugString(L"" __FUNCTION__ " Adding MidiFlowOut scheduler");

            // Our clientConnectionPipe is now the Scheduler
            RETURN_IF_FAILED(GetMidiScheduler(
                BindingHandle, 
                MidiFlowOut, 
                devicePipe, 
                newClientConnectionPipe, 
                clientConnectionPipe)); // clientConnectionPipe is the plugin

            clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());

            newClientConnectionPipe = clientConnectionPipe;
        }

        // no more transforms to add
        clientConnectionPipe = newClientConnectionPipe;

        //clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());

        RETURN_IF_FAILED(clientPipe->SetDataFormatOut(clientConnectionPipe->DataFormatOut()));
        Client->DataFormat = clientPipe->DataFormatOut();
        RETURN_IF_FAILED(clientPipe->AddConnectedPipe(clientConnectionPipe));

        clientConnectionPipe.reset();
        newClientConnectionPipe.reset();
    }

    m_SessionTracker->AddClientEndpointConnection(SessionId, MidiDevice);

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
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto lock = m_ClientManagerLock.lock();

    // locate this client in the list and clean it up, which will disconnect
    // itself from the device pipe.
    // After the client is cleaned up and released, locate the device pipe
    // which was used, and if it no longer has any associated clients, clean
    // it up as well.
    auto client = m_ClientPipes.find(ClientHandle);
    if (client != m_ClientPipes.end())
    {
        wil::com_ptr_nothrow<CMidiClientPipe> midiClientPipe = (CMidiClientPipe*)(client->second.get());

        midiClientPipe->Cleanup();

        m_SessionTracker->RemoveClientEndpointConnection(midiClientPipe->SessionId(), client->second->MidiDevice().c_str());

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


