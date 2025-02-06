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
    std::shared_ptr<CMidiPerformanceManager>& performanceManager,
    std::shared_ptr<CMidiProcessManager>& processManager,
    std::shared_ptr<CMidiDeviceManager>& deviceManager,
    std::shared_ptr<CMidiSessionTracker>& sessionTracker
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager = performanceManager;
    m_ProcessManager = processManager;
    m_DeviceManager = deviceManager;
    m_SessionTracker = sessionTracker;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT
CMidiClientManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto lock = m_ClientManagerLock.lock();

    m_PerformanceManager.reset();
    m_ProcessManager.reset();
    m_DeviceManager.reset();
    m_SessionTracker.reset();

    // tear down all transforms before we clean up the client/device
    for (auto const& transform : m_TransformPipes)
    {
        transform.second->Shutdown();
    }
    m_TransformPipes.clear();

    for (auto const& client : m_ClientPipes)
    {
        client.second->Shutdown();
    }
    m_ClientPipes.clear();

    for (auto const& device : m_DevicePipes)
    {
        device.second->Shutdown();
    }
    m_DevicePipes.clear();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    return S_OK;
}

HRESULT
GetEndpointRequiresOutboundProtocolDownscaling(
    _In_ std::wstring const& midiDevice, 
    _In_ MidiFlow const flow, 
    _In_ MidiDataFormats const deviceFormat, 
    _Inout_ bool& addProtocolDownscaler)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // default to false
    addProtocolDownscaler = false;

    if (deviceFormat == MidiDataFormats::MidiDataFormats_UMP && (flow == MidiFlow::MidiFlowBidirectional || flow == MidiFlow::MidiFlowOut))
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);

        auto deviceInfo = DeviceInformation::CreateFromIdAsync(
            midiDevice, additionalProperties, 
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_NativeDataFormat);
        if (prop)
        {
            auto nativeFormat = winrt::unbox_value<uint8_t>(prop);

            if (nativeFormat == MidiDataFormats_ByteStream)
            {
                // Native bytestream behind a UMP driver, so yes, we need to downscale because the driver will just discard MT4

                addProtocolDownscaler = true;
            }
        }
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(addProtocolDownscaler, "Requires downscaler")
    );

    return S_OK;
}


HRESULT
GetDeviceSupportedDataFormat(_In_ std::wstring midiDevice, _Inout_ MidiDataFormats& dataFormat)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_SupportedDataFormats);
    if (prop)
    {
        dataFormat = MidiDataFormats::MidiDataFormats_Any;
        try
        {
            // this interface is pointing to a UMP interface, so use that instance id.
            dataFormat = (MidiDataFormats)winrt::unbox_value<uint8_t>(prop);
        }
        CATCH_LOG();
    }
    else
    {
        // default to any
        dataFormat = MidiDataFormats::MidiDataFormats_Any;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(dataFormat, "MIDI Data Format")
    );


    return S_OK;
}

HRESULT
GetEndpointGenerateIncomingTimestamp(_In_ std::wstring midiDevice, _Inout_ bool& generateIncomingTimestamp, _In_ MidiFlow& flow)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // we only generate timestamps for incoming messages FROM the device
    if (flow == MidiFlow::MidiFlowBidirectional || flow == MidiFlow::MidiFlowIn)
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_GenerateIncomingTimestamp);
        auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_GenerateIncomingTimestamp);
        if (prop)
        {
            // this interface is pointing to a UMP interface, so use that instance id.
            generateIncomingTimestamp = winrt::unbox_value<bool>(prop);
        }
        else
        {
            // default to true
            generateIncomingTimestamp = true;
        }
    }
    else
    {
        // output-only flow
        generateIncomingTimestamp = false;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(generateIncomingTimestamp, "Generate Incoming Timestamp")
    );

    return S_OK;
}

HRESULT
GetEndpointAlias(_In_ LPCWSTR midiDevice, _In_ std::wstring& alias, _In_ MidiFlow& aliasFlow)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    alias = internal::NormalizeEndpointInterfaceIdWStringCopy(midiDevice);

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_AssociatedUMP);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_AssociatedUMP);
    if (prop)
    {
        // this interface is pointing to a UMP interface, so use that instance id.
        alias = internal::NormalizeEndpointInterfaceIdWStringCopy(winrt::unbox_value<winrt::hstring>(prop).c_str());
    }

    // retrieve the interface class for the aliased device, either it should have the same
    // flow as requested, or it should be bidi.
    //
    // If a client activates a bidi endpoint as in or out, this will ensure we activate it as bidi so
    // alternate flows will work as expected. This needs to be done whether or not the activated
    // interface has an AssociatedUMP or not.
    additionalProperties.Clear();
    additionalProperties.Append(L"System.Devices.InterfaceClassGuid");
    auto aliasedDeviceInfo = DeviceInformation::CreateFromIdAsync(alias, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();
    prop = aliasedDeviceInfo.Properties().Lookup(L"System.Devices.InterfaceClassGuid");
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    auto interfaceClass = winrt::unbox_value<winrt::guid>(prop);

    if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI) == interfaceClass)
    {
        aliasFlow = MidiFlowBidirectional;
    }
    // else assume the flow is unchanged, and we will catch any inconsistencies when we attempt to
    // initialize the device pipe for the requested flow.

    std::transform(alias.begin(), alias.end(), alias.begin(), ::towlower);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

HRESULT
GetEndpointGroupIndex(_In_ std::wstring midiDevice, _Inout_ BYTE& groupIndex)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // Default to the invalid group index, to indicate that no group index is available for
    // this midiDevice.
    groupIndex = INVALID_GROUP_INDEX;

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_PortAssignedGroupIndex);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_PortAssignedGroupIndex);
    if (prop)
    {
        try
        {
            // If a group index is provided by this device, it must be valid.
            groupIndex = (BYTE) winrt::unbox_value<UINT32>(prop);
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INDEX_OUT_OF_BOUNDS), !IS_VALID_GROUP_INDEX(groupIndex));
        }
        CATCH_LOG();
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(groupIndex, "Port group index")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiClient(
    LPCWSTR midiDevice,
    LPCWSTR activatedMidiDevice,
    GUID sessionId,
    DWORD clientProcessId,
    PMIDISRV_CLIENTCREATION_PARAMS creationParams,
    PMIDISRV_CLIENT client,
    wil::unique_handle& clientProcessHandle,
    wil::com_ptr_nothrow<CMidiPipe>&midiClientPipe,
    BOOL overwriteIncomingZeroTimestamps
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(sessionId, "Session Id"),
        TraceLoggingUInt32(clientProcessId, "clientProcessId")
    );

    wil::com_ptr_nothrow<CMidiClientPipe> clientPipe;
    BYTE groupIndex{0};

    // midiDevice points to the UMP endpoint being used, activatedMidiDevice points
    // to the interface used for activation by the client, which will be a midi 1
    // port if the client is using a legacy API.
    //
    // When activated on a Midi 1 port, the activated interface will contain the 
    // group index that the port is associated to, the midi client pipe needs that for message
    // filtering.
    RETURN_IF_FAILED(GetEndpointGroupIndex(activatedMidiDevice, groupIndex));

    auto cleanMidiDevice = internal::NormalizeEndpointInterfaceIdWStringCopy(midiDevice);

    // we have either a new device or an existing device, create the client pipe and connect to it.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiClientPipe>(&clientPipe));

    // Initialize our client
    RETURN_IF_FAILED(clientPipe->Initialize(clientProcessHandle.get(), cleanMidiDevice.c_str(), groupIndex, sessionId, clientProcessId, creationParams, client, &m_MmcssTaskId, overwriteIncomingZeroTimestamps));

    // Add this client to the client pipes list and set the output client handle
    client->ClientHandle = (MidiClientHandle)clientPipe.get();
    midiClientPipe = clientPipe.get();
    m_ClientPipes.emplace(client->ClientHandle, std::move(clientPipe));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanMidiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(sessionId, "Session Id")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiDevice(
    LPCWSTR midiDevice,
    PMIDISRV_CLIENTCREATION_PARAMS creationParams,
    wil::com_ptr_nothrow<CMidiPipe>& midiDevicePipe
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto cleanMidiDevice = internal::NormalizeEndpointInterfaceIdWStringCopy(midiDevice);

    // Get an existing device pipe if one exists, otherwise create a new pipe
    auto device = m_DevicePipes.find(cleanMidiDevice);
    if (device != m_DevicePipes.end())
    {
        RETURN_HR_IF(E_UNEXPECTED, device->second->Flow() != creationParams->Flow);
        midiDevicePipe = device->second;
    }
    else
    {
        wil::com_ptr_nothrow<CMidiDevicePipe> devicePipe;

        // The devicePipe must be initialized with the flow of the aliased device
        MIDISRV_DEVICECREATION_PARAMS deviceCreationParams{ };

        deviceCreationParams.BufferSize = creationParams->BufferSize;

        // create the device using the device's preferred format.
        // if that mismatches the client capabilities, a transform will be inserted.
        MidiDataFormats dataFormat = MidiDataFormats::MidiDataFormats_Any;
        LOG_IF_FAILED(GetDeviceSupportedDataFormat(cleanMidiDevice.c_str(), dataFormat));

        deviceCreationParams.DataFormat = dataFormat;
        deviceCreationParams.Flow = creationParams->Flow;

        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiDevicePipe>(&devicePipe));
        RETURN_IF_FAILED(devicePipe->Initialize(cleanMidiDevice.c_str(), &deviceCreationParams, &m_MmcssTaskId));

        midiDevicePipe = devicePipe.get();
        m_DevicePipes.emplace(cleanMidiDevice.c_str(), std::move(devicePipe));
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanMidiDevice.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiProtocolDownscalerTransform(
    _In_ MidiFlow flow,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& devicePipe,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& nextDeviceSidePipe,
    _In_ wil::com_ptr_nothrow<CMidiPipe>& clientConnectionPipe)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, devicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF(E_UNEXPECTED, flow != MidiFlow::MidiFlowOut);

    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

    auto transformGuid = _uuidof(Midi2UmpProtocolDownscalerTransform);

    // search existing transforms for this device for an output that supports
    // the requested flow and data format.
    auto transforms = m_TransformPipes.equal_range(devicePipe->MidiDevice());
    for (auto& transform = transforms.first; transform != transforms.second; ++transform)
    {
        if (flow == transform->second->Flow() && 
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

        creationParams.Flow = flow;
        creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
        creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_UMP;
        creationParams.TransformGuid = transformGuid;
        creationParams.UmpGroupIndex = INVALID_GROUP_INDEX;

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        wil::com_ptr_nothrow<IMidiDeviceManager> midiDeviceManager = m_DeviceManager.get();
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(devicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, midiDeviceManager.get()));
        transformPipe = transform.get();

        // connect the transform to the device
        RETURN_IF_FAILED(transformPipe->AddConnectedPipe(nextDeviceSidePipe));

        m_TransformPipes.emplace(devicePipe->MidiDevice(), transform);
    }

    clientConnectionPipe = transformPipe;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}




// This function handles data format translation only
_Use_decl_annotations_
HRESULT 
CMidiClientManager::GetMidiTransform(
    LPCWSTR activatedMidiDevice,
    MidiFlow flow,
    MidiDataFormats dataFormatFrom,
    MidiDataFormats dataFormatTo,
    wil::com_ptr_nothrow<CMidiPipe>& devicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& clientConnectionPipe
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, devicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };
    GUID transformGuid{ 0 };

    // no transform is required, this shouldn't have been called.
    RETURN_HR_IF(E_UNEXPECTED, dataFormatFrom == dataFormatTo);

    // First we need to identify which transform will be needed
    if (MidiDataFormats_UMP == dataFormatFrom &&
        MidiDataFormats_ByteStream == dataFormatTo)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Midi2UMP2BSTransform (UMP to Bytestream) required", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    
        transformGuid = __uuidof(Midi2UMP2BSTransform);
    }
    else if (MidiDataFormats_ByteStream == dataFormatFrom &&
        MidiDataFormats_UMP == dataFormatTo)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Midi2BS2UMPTransform (Bytestream to UMP) required", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    
        transformGuid = __uuidof(Midi2BS2UMPTransform);
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unsupported translation type requested. This is probably an error in specifying data format.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    
        // unknown transform
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    // Converting bytestream to UMP cannot be shared by multiple clients because
    // libmidi2 holds state between the bytestream messages, mixing messages from
    // different providers will cause corruption.
    // Skip looking for an existing BS to UMP transform, always use
    // a new one.
    if (transformGuid == __uuidof(Midi2BS2UMPTransform))
    {
        // UMP messages are targeted by message group, and like BytestreamToUMP takes in
        // a group index for targeting the produced UMP messages, UMP to Bytestream
        // provides a group index that the messages were targeted to. If we had a separate
        // UMP to Bytestream instance for every client, then UMP to Bytestream could perform 
        // the filtering to the clients group, however performing the same UMP to Bytestream
        // conversion for every client in the system would be extremely inefficient.
        //
        // For efficiency, UMP to Bytestream may be shared by multiple clients, but
        // the client pipe is responsible for filtering the messages by the group
        // index of the client, the group index of the message is provided to the client
        // pipe as the callback context.
        //
        // Search existing transforms for this device for an output that supports
        // the requested flow and data format.
        auto transforms = m_TransformPipes.equal_range(devicePipe->MidiDevice());
        for (auto& transform = transforms.first; transform != transforms.second; ++transform)
        {
            if (flow == transform->second->Flow() &&
                dataFormatFrom == transform->second->DataFormatIn() &&
                dataFormatTo == transform->second->DataFormatOut())
            {
                RETURN_HR_IF(E_UNEXPECTED, transformPipe);
                transformPipe = transform->second;
            }
        }
    }

    // not found, instantiate the transform that is needed.
    if (!transformPipe)
    {
        BYTE groupIndex{0};
        MIDISRV_TRANSFORMCREATION_PARAMS creationParams { };

        // MidiDevice points to the UMP endpoint being used, activatedMidiDevice points
        // to the interface used for activation by the client, which will be a midi 1
        // port if the client is using a legacy API.
        //
        // When activated on a Midi 1 port, the activated interface will contain the 
        // group index that the port is associated to, the midi transform will need
        // that for message targeting.
        RETURN_IF_FAILED(GetEndpointGroupIndex(activatedMidiDevice, groupIndex));

        creationParams.Flow = flow;
        creationParams.DataFormatIn = dataFormatFrom;
        creationParams.DataFormatOut = dataFormatTo;
        creationParams.UmpGroupIndex = groupIndex;
        creationParams.TransformGuid = transformGuid;

        if (MidiDataFormats_UMP == dataFormatFrom &&
            MidiDataFormats_ByteStream == dataFormatTo)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Adding Midi2UMP2BSTransform (UMP to Bytestream)", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            creationParams.TransformGuid = __uuidof(Midi2UMP2BSTransform);
        }
        else if (MidiDataFormats_ByteStream == dataFormatFrom &&
            MidiDataFormats_UMP == dataFormatTo)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Adding Midi2BS2UMPTransform (Bytestream to UMP)", MIDI_TRACE_EVENT_MESSAGE_FIELD)
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
                TraceLoggingWideString(L"Unsupported translation type requested. This is probably an error in specifying data format.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // unknown transform
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        wil::com_ptr_nothrow<IMidiDeviceManager> midiDeviceManager = m_DeviceManager.get();
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(devicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, midiDeviceManager.get()));
        transformPipe = transform.get();

        // connect the transform to the device
        if (flow == MidiFlowIn)
        {
            RETURN_IF_FAILED(devicePipe->AddConnectedPipe(transformPipe));
        }
        else
        {
            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(devicePipe));
        }

        //m_TransformPipes.emplace(devicePipe->MidiDevice(), transformPipe);
        m_TransformPipes.emplace(devicePipe->MidiDevice(), transform);
    }

    clientConnectionPipe = transformPipe;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


// This isn't a generic plugin "get" function because scheduling is a system-managed feature, 
// like data format translation and JR Timestamp management. Also, like those, only one can be
// associated with any given connection / device. 
_Use_decl_annotations_
HRESULT
CMidiClientManager::GetMidiScheduler(
    MidiFlow flow,
    wil::com_ptr_nothrow<CMidiPipe>& devicePipe,
    wil::com_ptr_nothrow<CMidiPipe>& nextDeviceSidePipe,
    wil::com_ptr_nothrow<CMidiPipe>& clientConnectionPipe
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, devicePipe);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    wil::com_ptr_nothrow<CMidiPipe> transformPipe{ nullptr };

    // we only schedule outgoing messages
    RETURN_HR_IF(E_UNEXPECTED, flow != MidiFlow::MidiFlowOut);

    // Order of some transforms is important:
    // MIDI 1/2 New ACX Device: [All other transforms]->[Scheduler]->[JR Timestamps]->[UMP Device]
    // MIDI 1 Legacy KS Device: [All other transforms]->[Scheduler]->[Data Format Translation]->[Byte Stream Device]

    // As long as the format is correct, we always add the scheduler, but a timestamp of 0 will bypass it.
    // This will allow us to potentially add scheduling to legacy APIs if there was already planned support
    // there. Not a v1 thing due to potential compat behavior issues. We may decide not to do this at all.

    // See if we already have a scheduler attached for this device
    // if so, return that, because we only want a single scheduler per device
    auto transforms = m_TransformPipes.equal_range(devicePipe->MidiDevice());
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
            TraceLoggingWideString(L"Creating new scheduler", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        MIDISRV_TRANSFORMCREATION_PARAMS creationParams{ 0 };

        creationParams.Flow = flow;
        creationParams.DataFormatIn = MidiDataFormats::MidiDataFormats_UMP;
        creationParams.DataFormatOut = MidiDataFormats::MidiDataFormats_UMP;
        creationParams.TransformGuid = __uuidof(Midi2SchedulerTransform);
        creationParams.UmpGroupIndex = INVALID_GROUP_INDEX;

        // create the transform
        wil::com_ptr_nothrow<CMidiTransformPipe> transform;
        wil::com_ptr_nothrow<IMidiDeviceManager> midiDeviceManager = m_DeviceManager.get();
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiTransformPipe>(&transform));

        RETURN_IF_FAILED(transform->Initialize(devicePipe->MidiDevice().c_str(), &creationParams, &m_MmcssTaskId, midiDeviceManager.get()));

        transformPipe = transform.get();

        //// connect the transform to the device
        //if (flow == MidiFlowIn)
        //{
        //    RETURN_IF_FAILED(nextDeviceSidePipe->AddConnectedPipe(transformPipe));
        //}
        //else
        //{
            RETURN_IF_FAILED(transformPipe->AddConnectedPipe(nextDeviceSidePipe));
        //}

        //m_TransformPipes.emplace(devicePipe->MidiDevice(), transformPipe);
        m_TransformPipes.emplace(devicePipe->MidiDevice(), transform);
    }

    clientConnectionPipe = transformPipe;

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(devicePipe->MidiDevice().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiClientManager::CreateMidiClient(
    LPCWSTR midiDevice,
    GUID sessionId,
    DWORD clientProcessId,
    wil::unique_handle& clientProcessHandle,
    PMIDISRV_CLIENTCREATION_PARAMS creationParams,
    PMIDISRV_CLIENT client,
    BOOL internalProtocolNegotiationUseOnly
)
{
    // Reject invalid data flows
    RETURN_HR_IF(E_INVALIDARG, creationParams->Flow != MidiFlowIn && 
                                creationParams->Flow != MidiFlowOut && 
                                creationParams->Flow != MidiFlowBidirectional);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId, "Session Id"),
        TraceLoggingUInt32(clientProcessId, "Client Process Id"),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    auto sessionCheckHR = m_SessionTracker->IsValidSession(sessionId, clientProcessId);
    if (FAILED(sessionCheckHR))
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Session is invalid for this client process. Cannot create MIDI client.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(sessionId, "Session Id"),
            TraceLoggingUInt32(clientProcessId, "Client Process Id"),
            TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(sessionCheckHR);
    }

    if (internalProtocolNegotiationUseOnly && creationParams->DataFormat != MidiDataFormats::MidiDataFormats_UMP)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Called for internal protocol negotiation, but creation params dataformat is not UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(sessionId, "Session Id"),
            TraceLoggingUInt32(clientProcessId, "Client Process Id"),
            TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(E_UNEXPECTED);
    }

    if (internalProtocolNegotiationUseOnly && creationParams->Flow != MidiFlow::MidiFlowBidirectional)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Called for internal protocol negotiation, but creation params data flow is not Bidirectional", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(sessionId, "Session Id"),
            TraceLoggingUInt32(clientProcessId, "Client Process Id"),
            TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(E_UNEXPECTED);
    }

    auto lock = m_ClientManagerLock.lock();

    wil::com_ptr_nothrow<CMidiPipe> clientPipe;
    wil::com_ptr_nothrow<CMidiPipe> devicePipe;
    wil::com_ptr_nothrow<CMidiPipe> clientConnectionPipe;

    unique_mmcss_handle MmcssHandle;
    std::wstring primaryMidiDevice;

    //bool addMetadataListenerToIncomingStream{ true };
    bool generateIncomingMessageTimestamps{ true };

    // take a copy of the client creation params, to modify for device creation.
    MIDISRV_CLIENTCREATION_PARAMS deviceCreationParams = *creationParams;

    if (!internalProtocolNegotiationUseOnly)
    {
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
        RETURN_IF_FAILED(GetEndpointAlias(midiDevice, primaryMidiDevice, deviceCreationParams.Flow));

        // see if we should create timestamps or not. This option is set
        // by the transport at endpoint enumeration time. For most endpoint
        // types, this will be true, but for diagnostics loopback endpoints, 
        // it is false. So, we store the value in the property store to make
        // it more flexible
        RETURN_IF_FAILED(GetEndpointGenerateIncomingTimestamp(primaryMidiDevice, generateIncomingMessageTimestamps, creationParams->Flow));

        // Some endpoints, like the device-side of an app-to-app MIDI connection, 
        // should not have metadata listeners. This flag controls whether or not
        // we add one if otherwise eligible. 
        //RETURN_IF_FAILED(GetEndpointShouldHaveMetadataHandler(primaryMidiDevice, addMetadataListenerToIncomingStream, creationParams->Flow));

        RETURN_IF_FAILED(GetMidiClient(primaryMidiDevice.c_str(), midiDevice, sessionId, clientProcessId, creationParams, client, clientProcessHandle, clientPipe, generateIncomingMessageTimestamps));
    }
    else
    {
        // for protocol negotiation / metadata capture only.
        // NOTE: clientProcessHandle here is invalid
        RETURN_IF_FAILED(GetMidiClient(midiDevice, midiDevice, sessionId, clientProcessId, creationParams, client, clientProcessHandle, clientPipe, false));
    }

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        // If a new device has been created and added to m_DevicePipes,
        // removing the client will also remove the unused device
        LOG_IF_FAILED(DestroyMidiClient(client->ClientHandle));
        client->ClientHandle = NULL;
        client->DataFormat = MidiDataFormats_Invalid;
    });

    RETURN_IF_FAILED(GetMidiDevice(primaryMidiDevice.c_str(), &deviceCreationParams, devicePipe));
    devicePipe->AddClient((MidiClientHandle)clientPipe.get());

    // MidiFlowIn on the client flows data from the midi device to the client,
    // so we register the clientPipe to receive the callbacks from the clientConnectionPipe.
    if (clientPipe->IsFlowSupported(MidiFlowIn))
    {
        // Assume direct connection to the device
        clientConnectionPipe = devicePipe;

        if (!clientPipe->IsFormatSupportedIn(clientConnectionPipe->DataFormatIn()))
        {
            wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe;

            // client requires a specific format, retrieve the transform required for that format.
            RETURN_IF_FAILED(GetMidiTransform(
                midiDevice,
                MidiFlowIn,
                clientConnectionPipe->DataFormatIn(),
                clientPipe->DataFormatIn(),
                clientConnectionPipe,
                newClientConnectionPipe)); // clientConnectionPipe is the plugin

            newClientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
            clientConnectionPipe = newClientConnectionPipe;
        }

        RETURN_IF_FAILED(clientPipe->SetDataFormatIn(clientConnectionPipe->DataFormatOut()));
        client->DataFormat = clientPipe->DataFormatIn();
        RETURN_IF_FAILED(clientConnectionPipe->AddConnectedPipe(clientPipe));
        clientConnectionPipe.reset();
    }

    // MidiFlowOut on the client flows data from the midi client to the device,
    // so we register the clientConnectionPipe to receive the callbacks from the clientPipe.
    if (clientPipe->IsFlowSupported(MidiFlowOut))
    {
        // assume direct connection to device pipe
        clientConnectionPipe = devicePipe;

        if (!internalProtocolNegotiationUseOnly)
        {
            // Protocol Translator ----------------------------------------------------------------
            // This translates MT4 to MT2 for MIDI 1.0 devices connected through the new driver.
            // For devices connected directly to the service via the old driver, we don't have to
            // do any translation or scaling because that's all taken care of in the BS2UMP and 
            // UMP2BS transforms. (They do format translation but *also* MT4/MT2 translation)
            // This outbound translation step should happen prior to the outbound scheduler step.

            bool addProtocolDownscalerForMidi1DeviceWithUmpDriver{ false };
            RETURN_IF_FAILED(GetEndpointRequiresOutboundProtocolDownscaling(
                primaryMidiDevice,
                creationParams->Flow,
                devicePipe->DataFormatOut(),
                addProtocolDownscalerForMidi1DeviceWithUmpDriver));

            if (addProtocolDownscalerForMidi1DeviceWithUmpDriver)
            {
                wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe;

                RETURN_IF_FAILED(GetMidiProtocolDownscalerTransform(
                    MidiFlowOut,
                    devicePipe,
                    clientConnectionPipe,
                    newClientConnectionPipe));

                newClientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
                clientConnectionPipe = newClientConnectionPipe;
            }

            // Data Format Translator, performed after the MT4 to MT2 translation,
            // if that was required.
            if (!clientPipe->IsFormatSupportedOut(clientConnectionPipe->DataFormatOut()))
            {
                wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe;

                // Format is not supported, so we need to transform
                // client requires a specific format, retrieve the transform required for that format.
                // Our clientConnectionPipe is now the format translator

                RETURN_IF_FAILED(GetMidiTransform(
                    midiDevice,
                    MidiFlowOut,
                    clientPipe->DataFormatOut(),
                    clientConnectionPipe->DataFormatOut(),
                    clientConnectionPipe,
                    newClientConnectionPipe)); // clientConnectionPipe is the plugin

                newClientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
                clientConnectionPipe = newClientConnectionPipe;
            }

            // Scheduler ----------------------------------------------------------------
            // for now, the scheduler is only going to work with UMP, so we hope
            // any required translation is done BEFORE we add this.
            // We need to check what format gets sent out of the transform and into the
            // next pipe, so check the input of that pipe for UWP support.
            if (clientConnectionPipe->IsFormatSupportedIn(MidiDataFormats::MidiDataFormats_UMP))
            {
                wil::com_ptr_nothrow<CMidiPipe> newClientConnectionPipe;

                // Our clientConnectionPipe is now the Scheduler
                RETURN_IF_FAILED(GetMidiScheduler(
                    MidiFlowOut,
                    devicePipe,
                    clientConnectionPipe,
                    newClientConnectionPipe));

                newClientConnectionPipe->AddClient((MidiClientHandle)clientPipe.get());
                clientConnectionPipe = newClientConnectionPipe;
            }
        }
        else
        {
            // for protocol negotiation use only, so no translator, scheduler, or downscaler
        }

        RETURN_IF_FAILED(clientPipe->SetDataFormatOut(clientConnectionPipe->DataFormatIn()));
        client->DataFormat = clientPipe->DataFormatOut();
        RETURN_IF_FAILED(clientPipe->AddConnectedPipe(clientConnectionPipe));
        clientConnectionPipe.reset();
    }

    RETURN_IF_FAILED(m_SessionTracker->AddClientEndpointConnection(sessionId, clientProcessId, midiDevice, client->ClientHandle));

    cleanupOnFailure.release();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Complete.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId, "Session Id"),
        TraceLoggingUInt32(clientProcessId, "Client Process Id"),
        TraceLoggingWideString(midiDevice, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientManager::DestroyMidiClient(
    MidiClientHandle clientHandle
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt64(clientHandle, "client handle")
    );

    auto lock = m_ClientManagerLock.lock();

    // locate this client in the list and clean it up, which will disconnect
    // itself from the device pipe.
    // After the client is cleaned up and released, locate the device pipe
    // which was used, and if it no longer has any associated clients, clean
    // it up as well.
    auto client = m_ClientPipes.find(clientHandle);
    if (client != m_ClientPipes.end())
    {
        wil::com_ptr_nothrow<CMidiClientPipe> midiClientPipe = (CMidiClientPipe*)(client->second.get());
        wil::com_ptr_nothrow<CMidiPipe> clientAsMidiPipe = midiClientPipe.get();

        m_SessionTracker->RemoveClientEndpointConnection(midiClientPipe->SessionId(), midiClientPipe->ClientProcessId(), client->second->MidiDevice().c_str(), clientHandle);

        midiClientPipe->Shutdown();

        // remove this client from all of the transforms and disconnect it and any transforms no longer in use
        for (auto transform = m_TransformPipes.begin(); transform != m_TransformPipes.end();)
        {
            wil::com_ptr_nothrow<CMidiPipe> midiTransformPipe = transform->second.get();

            // Remove the connection between the client and the transform, if connected.
            //
            // Also unregister this client as a client of this tranform, it may be
            // a client of this transform without being directly attached to it.
            midiTransformPipe->RemoveConnectedPipe(clientAsMidiPipe);
            midiTransformPipe->RemoveClient(clientHandle);

            // if this transform is no longer in use, has no clients associated to it,
            // disconnect it from any devices or other transforms it may have been connected to,
            // and clean it up.
            if (!midiTransformPipe->InUse())
            {
                for (auto connection = m_DevicePipes.begin(); connection != m_DevicePipes.end();++connection)
                {
                    connection->second->RemoveConnectedPipe(midiTransformPipe);
                }
                for (auto connection = m_TransformPipes.begin(); connection != m_TransformPipes.end();++connection)
                {
                    connection->second->RemoveConnectedPipe(midiTransformPipe);
                }

                midiTransformPipe->Shutdown();
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

            // Remove the connection between the client and the device, if connected.
            //
            // Also unregister this client as a client of this device, it may be
            // a client of this device without being directly attached to it.
            midiDevicePipe->RemoveConnectedPipe(clientAsMidiPipe);
            midiDevicePipe->RemoveClient(clientHandle);

            // if this device is no longer in use, has no clients associated to it,
            // clean it up.
            if (!midiDevicePipe->InUse())
            {
                midiDevicePipe->Shutdown();
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
        RETURN_IF_FAILED(E_INVALIDARG);
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt64(clientHandle, "client handle")
    );


    return S_OK;
}
