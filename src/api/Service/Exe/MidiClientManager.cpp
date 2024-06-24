// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


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

    return S_OK;
}

HRESULT
GetEndpointRequiresOutboundProtocolDownscaling(
    _In_ std::wstring const& MidiDevice, 
    _In_ MidiFlow const Flow, 
    _In_ MidiDataFormat const DeviceFormat, 
    _Inout_ bool& AddProtocolDownscaler)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id")
    );

    // default to false
    AddProtocolDownscaler = false;

    if (DeviceFormat == MidiDataFormat::MidiDataFormat_UMP && (Flow == MidiFlow::MidiFlowBidirectional || Flow == MidiFlow::MidiFlowOut))
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_NativeDataFormat));

        auto deviceInfo = DeviceInformation::CreateFromIdAsync(
            MidiDevice, additionalProperties, 
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_NativeDataFormat));
        if (prop)
        {
            // this interface is pointing to a UMP interface, so use that instance id.
            auto nativeFormat = winrt::unbox_value<uint8_t>(prop);

            if (nativeFormat == MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM)
            {
                // Native bytestream behind a UMP driver, so yes, we need to downscale because the driver will just discard MT4

                AddProtocolDownscaler = true;
            }
        }
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id"),
        TraceLoggingBool(AddProtocolDownscaler, "Requires downscaler")
    );

    return S_OK;
}

//HRESULT
//GetEndpointShouldHaveMetadataHandler(_In_ std::wstring MidiDevice, _Inout_ bool& AddMetadataListener, _In_ MidiFlow Flow)
//{
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        __FUNCTION__,
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingWideString(MidiDevice.c_str(), "Device Id")
//    );
//
//    if (Flow == MidiFlow::MidiFlowBidirectional || Flow == MidiFlow::MidiFlowIn)
//    {
//        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
//        additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler));
//        auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();
//
//        auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_EndpointRequiresMetadataHandler));
//        if (prop)
//        {
//            // this interface is pointing to a UMP interface, so use that instance id.
//            AddMetadataListener = winrt::unbox_value<bool>(prop);
//        }
//        else
//        {
//            // default to true
//            AddMetadataListener = true;
//        }
//    }
//    else
//    {
//        // output-only flow
//        AddMetadataListener = false;
//    }
//
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        __FUNCTION__,
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingWideString(MidiDevice.c_str(), "Device Id"),
//        TraceLoggingBool(AddMetadataListener, "Add Metadata Listener")
//    );
//
//    return S_OK;
//}


HRESULT
GetDeviceSupportedDataFormat(_In_ std::wstring MidiDevice, _Inout_ MidiDataFormat& DataFormat)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id")
    );

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
    if (prop)
    {
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
        // default to any
        DataFormat = MidiDataFormat::MidiDataFormat_Any;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id"),
        TraceLoggingBool(DataFormat, "MIDI Data Format")
    );

    return S_OK;
}

HRESULT
GetEndpointGenerateIncomingTimestamp(_In_ std::wstring MidiDevice, _Inout_ bool& GenerateIncomingTimestamp, _In_ MidiFlow& Flow)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id")
    );

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

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice.c_str(), "Device Id"),
        TraceLoggingBool(GenerateIncomingTimestamp, "Generate Incoming Timestamp")
    );

    return S_OK;
}

HRESULT
GetEndpointAlias(_In_ LPCWSTR MidiDevice, _In_ std::wstring& Alias, _In_ MidiFlow& AliasFlow)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(MidiDevice, "Device Id")
    );

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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice, "Device Id"),
        TraceLoggingGuid(SessionId, "Session Id")
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice, "Device Id")
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


_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiProtocolDownscalerTransform(
    _In_ handle_t BindingHandle,
    _In_ MidiFlow Flow,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& NextDeviceSidePipe,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, DevicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DevicePipe->MidiDevice().c_str(), "Device Id")
    );

    RETURN_HR_IF(E_UNEXPECTED, Flow != MidiFlow::MidiFlowOut);

    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

    auto transformGuid = _uuidof(Midi2UmpProtocolDownscalerTransform);

    // search existing transforms for this device for an output that supports
    // the requested flow and data format.
    auto transforms = m_TransformPipes.equal_range(DevicePipe->MidiDevice());
    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
    {
        if (Flow == transform->second->Flow() && 
            transform->second->TransformGuid() == transformGuid)
        {
            RETURN_HR_IF(E_UNEXPECTED, transformPipe);
            transformPipe = transform->second;
        }
    }

    // connect transform to device because we're transforming for the case where 
    // we have a MIDI 1.0 device connected to the new driver, and we need to 
    // transform MT4 messages to MT2 messages

    if (!transformPipe)
    {
        MIDISRV_TRANSFORMCREATION_PARAMS creationParams{ };

        creationParams.Flow = Flow;
        creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_UMP;
        creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;
        creationParams.TransformGuid = transformGuid;

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, (IUnknown*)&m_DeviceManager));
        transformPipe = transform.get();

        // connect the transform to the device
        RETURN_IF_FAILED(transformPipe->AddConnectedPipe(NextDeviceSidePipe));

        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transform);
    }

    ClientConnectionPipe = transformPipe;

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
    RETURN_HR_IF_NULL(E_INVALIDARG, DevicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DevicePipe->MidiDevice().c_str(), "Device Id")
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
        MIDISRV_TRANSFORMCREATION_PARAMS creationParams { };

        creationParams.Flow = Flow;
        creationParams.DataFormatIn = DataFormatFrom;
        creationParams.DataFormatOut = DataFormatTo;

        if (MidiDataFormat_UMP == DataFormatFrom &&
            MidiDataFormat_ByteStream == DataFormatTo)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
    RETURN_HR_IF_NULL(E_INVALIDARG, DevicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DevicePipe->MidiDevice().c_str(), "Device Id")
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
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(DevicePipe->MidiDevice().c_str(), "Device Id"),
            TraceLoggingWideString(L"Creating new scheduler", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

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

//_Use_decl_annotations_
//HRESULT
//CMidiClientManager::GetMidiEndpointMetadataHandler(
//    handle_t BindingHandle,
//    MidiFlow Flow,
//    wil::com_ptr_nothrow<CMidiPipe>& DevicePipe,
//    wil::com_ptr_nothrow<CMidiPipe>& NextDeviceSidePipe,
//    wil::com_ptr_nothrow<CMidiPipe>& ClientConnectionPipe
//)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, DevicePipe);
//
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this"),
//        TraceLoggingWideString(DevicePipe->MidiDevice().c_str(), "Device Id")
//    );
//
//    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };
//
//    // we only handle metadata on incoming messages
//    RETURN_HR_IF(E_UNEXPECTED, Flow != MidiFlow::MidiFlowIn);
//
//
//    auto transforms = m_TransformPipes.equal_range(DevicePipe->MidiDevice());
//    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
//    {
//        //wil::com_ptr_nothrow<CMidiTransformPipe> pipe = transform->second.get();
//
//        if (transform->second->TransformGuid() == __uuidof(Midi2EndpointMetadataListenerTransform))
//        {
//            transformPipe = transform->second;
//            break;
//        }
//    }
//
//    // not found, instantiate the transform that is needed.
//    if (!transformPipe)
//    {
//        MIDISRV_TRANSFORMCREATION_PARAMS creationParams{ 0 };
//
//        creationParams.Flow = Flow;
//        creationParams.DataFormatIn = MidiDataFormat::MidiDataFormat_UMP;
//        creationParams.DataFormatOut = MidiDataFormat::MidiDataFormat_UMP;
//
//        creationParams.TransformGuid = __uuidof(Midi2EndpointMetadataListenerTransform);
//
//        // create the transform
//        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
//        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));
//
//        RETURN_IF_FAILED(transform->Initialize(BindingHandle, DevicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, (IUnknown*)m_DeviceManager.get()));
//
//        transformPipe = transform.get();
//
//        // connect the transform to the device
//
//        if (Flow == MidiFlowIn)
//        {
//            RETURN_IF_FAILED(NextDeviceSidePipe->AddConnectedPipe(transformPipe));
//        }
//        else
//        {
//            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(NextDeviceSidePipe));
//        }
//
//        m_TransformPipes.emplace(DevicePipe->MidiDevice(), transform);
//    }
//
//    ClientConnectionPipe = transformPipe;
//
//    return S_OK;
//}



_Use_decl_annotations_
HRESULT
CMidiClientManager::CreateMidiClient(
    handle_t BindingHandle,
    LPCWSTR MidiDevice,
    GUID SessionId,
    DWORD ClientProcessId,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client,
    BOOL InternalProtocolNegotiationUseOnly
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(MidiDevice),
        TraceLoggingGuid(SessionId)
    );

    // TODO: Need to pass in the client process id
    RETURN_IF_FAILED(m_SessionTracker->IsValidSession(SessionId, ClientProcessId));


    //if (InternalProtocolNegotiationUseOnly && Client->DataFormat != MidiDataFormat::MidiDataFormat_UMP)
    //{
    //    TraceLoggingWrite(
    //        MidiSrvTelemetryProvider::Provider(),
    //        MIDI_TRACE_EVENT_ERROR,
    //        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //        TraceLoggingPointer(this, "this"),
    //        TraceLoggingWideString(MidiDevice),
    //        TraceLoggingGuid(SessionId),
    //        TraceLoggingWideString(L"Called for internal protocol negotiation, but client dataformat is not UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    //    );
    //        
    //    return E_UNEXPECTED;
    //}

    if (InternalProtocolNegotiationUseOnly && CreationParams->DataFormat != MidiDataFormat::MidiDataFormat_UMP)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(MidiDevice),
            TraceLoggingGuid(SessionId),
            TraceLoggingWideString(L"Called for internal protocol negotiation, but creation params dataformat is not UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return E_UNEXPECTED;
    }

    if (InternalProtocolNegotiationUseOnly && CreationParams->Flow != MidiFlow::MidiFlowBidirectional)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(MidiDevice),
            TraceLoggingGuid(SessionId),
            TraceLoggingWideString(L"Called for internal protocol negotiation, but creation params data flow is not Bidirectional", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return E_UNEXPECTED;
    }


    auto lock = m_ClientManagerLock.lock();

    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;
    wil::com_ptr_nothrow<CMidiPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiPipe> devicePipe;
    wil::com_ptr_nothrow<CMidiPipe> clientConnectionPipe;

    wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe{ nullptr };

    unique_mmcss_handle MmcssHandle;
    std::wstring midiDevice;

    //bool addMetadataListenerToIncomingStream{ true };
    bool generateIncomingMessageTimestamps{ true };

    if (!InternalProtocolNegotiationUseOnly)
    {
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
        RETURN_IF_FAILED(GetEndpointGenerateIncomingTimestamp(midiDevice, generateIncomingMessageTimestamps, CreationParams->Flow));

        // Some endpoints, like the device-side of an app-to-app MIDI connection, 
        // should not have metadata listeners. This flag controls whether or not
        // we add one if otherwise eligible. 
        //RETURN_IF_FAILED(GetEndpointShouldHaveMetadataHandler(midiDevice, addMetadataListenerToIncomingStream, CreationParams->Flow));

        RETURN_IF_FAILED(GetMidiClient(BindingHandle, midiDevice.c_str(), SessionId, CreationParams, Client, clientProcessHandle, clientPipe, generateIncomingMessageTimestamps));
    }
    else
    {
        // for protocol negotiation / metadata capture only. BindingHandle here is going to be invalid
        // NOTE: clientProcessHandle here is invalid
        RETURN_IF_FAILED(GetMidiClient(BindingHandle, MidiDevice, SessionId, CreationParams, Client, clientProcessHandle, clientPipe, false));
    }

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


        //// Metadata Listener ----------------------------------------------------------------
        //// We should check protocol, not just data format here because we're putting this on
        //// MIDI 1.0 devices that use the new driver, and there's no reason to do that.
        //if (addMetadataListenerToIncomingStream && devicePipe->IsFormatSupportedIn(MidiDataFormat::MidiDataFormat_UMP))
        //{
        //    // Our clientConnectionPipe is now the Scheduler
        //    RETURN_IF_FAILED(GetMidiEndpointMetadataHandler(
        //        BindingHandle,
        //        MidiFlowIn,
        //        devicePipe,
        //        newClientConnectionPipe,
        //        clientConnectionPipe)); // clientConnectionPipe is the plugin

        //    clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
        //}
        //newClientConnectionPipe = clientConnectionPipe;


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
        // our initial state is straight through to the device pipe
        clientConnectionPipe = devicePipe;
        newClientConnectionPipe = devicePipe;

        if (!InternalProtocolNegotiationUseOnly)
        {
            // TODO: This needs to work with both bytestream and UMP clients, so this logic needs to change

                // Data Format Translator ---------------------------------------------------
            if (!clientPipe->IsFormatSupportedOut(newClientConnectionPipe->DataFormatOut()))
            {
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


            // Protocol Translator ----------------------------------------------------------------
            // This translates MT4 to MT2 for MIDI 1.0 devices connected through the new driver.
            // For devices connected directly to the service via the old driver, we don't have to
            // do any translation or scaling because that's all taken care of in the BS2UMP and 
            // UMP2BS transforms. (They do format translation but *also* MT4/MT2 translation)
            // This outbound translation step should happen prior to the outbound scheduler step.

            bool addProtocolDownscalerForMidi1DeviceWithUmpDriver{ false };
            RETURN_IF_FAILED(GetEndpointRequiresOutboundProtocolDownscaling(
                midiDevice,
                CreationParams->Flow,
                devicePipe->DataFormatOut(),
                addProtocolDownscalerForMidi1DeviceWithUmpDriver));

            if (addProtocolDownscalerForMidi1DeviceWithUmpDriver)
            {
                RETURN_IF_FAILED(GetMidiProtocolDownscalerTransform(
                    BindingHandle,
                    MidiFlowOut,
                    devicePipe,
                    newClientConnectionPipe,
                    clientConnectionPipe)); // clientConnectionPipe is the plugin

                clientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());

                newClientConnectionPipe = clientConnectionPipe;
            }
        }
        else
        {
            // for protocol negotiation use only, so no translator, scheduler, or downscaler
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

    // TODO: We need to verify the valid session before allowing client creation
    // This will keep apps from bypassing this feature, which is an important
    // requested feature from users.

    m_SessionTracker->AddClientEndpointConnection(SessionId, MidiDevice, Client->ClientHandle);

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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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

        m_SessionTracker->RemoveClientEndpointConnection(midiClientPipe->SessionId(), client->second->MidiDevice().c_str(), ClientHandle);

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


