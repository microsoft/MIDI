// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

namespace
{
    json::JsonObject BuildAvailableDeviceJson(_In_ MidiBleProtocol::DiscoveredDevice const& device)
    {
        json::JsonObject deviceJson;

        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY, json::JsonValue::CreateStringValue(device.Id));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY, json::JsonValue::CreateStringValue(device.Name));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SELECTED_PROTOCOL_KEY, json::JsonValue::CreateStringValue(MidiBleUtilities::ProtocolToJsonString(device.SelectedProtocol)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_KEY, json::JsonValue::CreateStringValue(MidiBleUtilities::NativeDataFormatToJsonString(device.NativeDataFormat)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY, json::JsonValue::CreateBooleanValue(device.IsConnected));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY, json::JsonValue::CreateBooleanValue(device.IsPaired));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SIGNAL_STRENGTH_KEY, json::JsonValue::CreateNumberValue(device.LastSignalStrengthDbm));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY, json::JsonValue::CreateStringValue(device.EndpointDeviceId));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY, json::JsonValue::CreateStringValue(device.EndpointDeviceInstanceId));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_KEY, json::JsonValue::CreateStringValue(device.LastConnectErrorDetail));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_HRESULT_KEY, json::JsonValue::CreateNumberValue(device.LastConnectErrorHresult));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.MessagesReceived)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.MessagesSent)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEND_ERROR_HRESULT_KEY, json::JsonValue::CreateNumberValue(device.LastSendErrorHresult));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PRESENT_KEY, json::JsonValue::CreateBooleanValue(device.IsPresent));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEEN_AGO_MS_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.LastSeenAgoMilliseconds)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_ENDPOINT_KEY, json::JsonValue::CreateBooleanValue(device.HasEndpoint));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY, json::JsonValue::CreateNumberValue(device.ConnectionIntervalUnits * 1.25));

        return deviceJson;
    }

    winrt::hstring GetCommandArgument(_In_ internal::MidiTransportCommandHelper& commandHelper, _In_ std::wstring const& key)
    {
        auto arg = commandHelper.Arguments()->find(key);

        if (arg != commandHelper.Arguments()->end())
        {
            return winrt::hstring{ arg->second };
        }

        return L"";
    }

    winrt::hstring GetCommandDeviceId(_In_ internal::MidiTransportCommandHelper& commandHelper)
    {
        return GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY);
    }

    MidiBleProtocol::Protocol ParseProtocolJsonString(
        _In_ winrt::hstring const& value,
        _In_ MidiBleProtocol::Protocol const defaultProtocol)
    {
        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP)
        {
            return MidiBleProtocol::Protocol::Midi2Ump;
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1)
        {
            return MidiBleProtocol::Protocol::Midi1;
        }

        return defaultProtocol;
    }

    void AddPeripheralStatusToResponse(_In_ json::JsonObject& responseObject)
    {
        json::JsonObject peripheralJson;

        auto peripheral = TransportState::Current().GetPeripheral();

        bool const isRunning = peripheral != nullptr && peripheral->IsRunning();

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_IS_RUNNING_KEY,
            json::JsonValue::CreateBooleanValue(isRunning));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY,
            json::JsonValue::CreateStringValue(
                MidiBleUtilities::ProtocolToJsonString(isRunning ? peripheral->Protocol() : MidiBleProtocol::Protocol::Unknown)));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ADVERTISED_NAME_KEY,
            json::JsonValue::CreateStringValue(isRunning ? peripheral->AdvertisedName() : L""));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_CLIENT_COUNT_KEY,
            json::JsonValue::CreateNumberValue(isRunning ? peripheral->SubscribedClientCount() : 0));

        // A remote Central being subscribed is the only sign that data can actually move.
        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY,
            json::JsonValue::CreateBooleanValue(isRunning && peripheral->IsClientSubscribed()));

        winrt::hstring endpointDeviceId{};
        winrt::hstring connectedDeviceName{};
        uint64_t messagesReceived{ 0 };
        uint64_t messagesSent{ 0 };

        if (isRunning)
        {
            // The connection exists only while a Central is subscribed, because the endpoint
            // represents the remote device rather than this PC.
            if (auto connection = peripheral->Connection())
            {
                endpointDeviceId = winrt::hstring{ connection->EndpointDeviceInterfaceId() };
                connectedDeviceName = connection->DeviceName();
                messagesReceived = connection->MessagesReceived();
                messagesSent = connection->MessagesSent();
            }
        }

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
            json::JsonValue::CreateStringValue(connectedDeviceName));

        auto const remoteClient = isRunning ? peripheral->RemoteClientInfo() : MidiBleRemoteClientInfo{};

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(remoteClient.Address));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_TYPE_KEY,
            json::JsonValue::CreateStringValue(remoteClient.AddressType));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_BLUETOOTH_DEVICE_ID_KEY,
            json::JsonValue::CreateStringValue(remoteClient.BluetoothDeviceId));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY,
            json::JsonValue::CreateBooleanValue(remoteClient.IsPaired));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_GENERIC_NAME_KEY,
            json::JsonValue::CreateBooleanValue(remoteClient.HasGenericName));

        // In this direction the remote Central picks the interval, so this shows what a phone or
        // tablet actually asks for rather than what Windows would request.
        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY,
            json::JsonValue::CreateNumberValue(isRunning ? peripheral->ConnectionIntervalUnits() * 1.25 : 0.0));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY,
            json::JsonValue::CreateStringValue(endpointDeviceId));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(messagesReceived)));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(messagesSent)));

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, peripheralJson);
    }

    void SetCommandHresultFailure(_In_ json::JsonObject& responseObject, _In_ HRESULT hr, _In_ std::wstring const& message)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, message);
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_RESULT_HRESULT_KEY, json::JsonValue::CreateNumberValue(static_cast<int32_t>(hr)));
    }

    // The configuration file lists the devices the user has already approved, so they are
    // reconnected without any app running. Discovery has to find each one first, so this only
    // queues the request; the endpoint manager connects when the device is seen.
    void QueueConfiguredDevices(_In_ json::JsonObject const& transportObject)
    {
        if (transportObject == nullptr || !transportObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY))
        {
            return;
        }

        auto devicesValue = transportObject.Lookup(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY);

        if (devicesValue == nullptr || devicesValue.ValueType() != json::JsonValueType::Array)
        {
            return;
        }

        auto endpointManager = TransportState::Current().GetEndpointManager();

        for (auto const& entry : devicesValue.GetArray())
        {
            auto deviceObject = entry.try_as<json::JsonObject>();

            if (deviceObject == nullptr)
            {
                continue;
            }

            if (!deviceObject.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY, true))
            {
                continue;
            }

            auto deviceId = deviceObject.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY, L"");

            if (deviceId.empty())
            {
                continue;
            }

            // Parked either way. The endpoint manager may not exist yet, and if it does it
            // drains this list again on the way up, so the id can never be dropped.
            TransportState::Current().AddConfiguredDeviceId(deviceId);

            if (endpointManager != nullptr && endpointManager->IsInitialized())
            {
                LOG_IF_FAILED(endpointManager->ConnectConfiguredDevices());
            }
        }
    }

    // Publishing this PC as a peripheral is off unless the configuration file asks for it, because
    // it makes the machine visible and connectable to anything nearby.
    void QueueConfiguredPeripheral(_In_ json::JsonObject const& transportObject)    {
        if (transportObject == nullptr)
        {
            return;
        }

        auto peripheralObject = transportObject.GetNamedObject(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, nullptr);

        if (peripheralObject == nullptr)
        {
            return;
        }

        if (!peripheralObject.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ENABLED_KEY, false))
        {
            return;
        }

        // MIDI 1.0 by default, because nothing on the market speaks BLE MIDI 2.0 yet and only one
        // characteristic can be published at a time.
        auto const protocol = ParseProtocolJsonString(
            peripheralObject.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY, L""),
            MidiBleProtocol::Protocol::Midi1);

        // Parked either way, for the same reason the device list is: the endpoint manager may not
        // exist yet, and it drains this on the way up.
        TransportState::Current().SetConfiguredPeripheralProtocol(protocol);

        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr && endpointManager->IsInitialized())
        {
            LOG_IF_FAILED(endpointManager->ConnectConfiguredDevices());
        }
    }
}

_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager
)
{
    UNREFERENCED_PARAMETER(transportId);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManager);


    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiConfigurationManager::ProcessEndpointCustomizations(
    json::JsonObject const& jsonObject,
    json::JsonObject& responseObject) noexcept
{
    UNREFERENCED_PARAMETER(responseObject);

    try
    {
        auto updateArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

        if (updateArray == nullptr || updateArray.Size() == 0)
        {
            return S_OK;
        }

        // Indexed rather than ranged, because windows.h renames IJsonValue::GetObject and
        // JsonArray::GetObjectAt is unaffected.
        for (uint32_t i = 0; i < updateArray.Size(); i++)
        {
            auto updateObject = updateArray.GetObjectAt(i);

            if (updateObject == nullptr)
            {
                continue;
            }

            auto matchObject = updateObject.GetNamedObject(
                WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey, nullptr);

            if (matchObject == nullptr)
            {
                // nothing to tie this customization to
                continue;
            }

            if (!updateObject.HasKey(WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey))
            {
                continue;
            }

            auto matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);

            // An image is a bare file name. A path here would let a configuration file point the
            // service at an arbitrary location.
            auto customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
                updateObject.GetNamedObject(WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey));

            if (matchCriteria == nullptr || customProperties == nullptr)
            {
                continue;
            }

            // Cached whether or not the endpoint exists yet. A Bluetooth endpoint is created only
            // once the device is in range and answers, which is usually long after this arrives,
            // and the creation path reads this cache before it activates the device node.
            LOG_HR_IF(E_FAIL, !m_customPropertiesCache->Add(matchCriteria, customProperties));

            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Cached endpoint customization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(customProperties->Name.c_str(), "custom name"),
                TraceLoggingWideString(matchCriteria->DeviceInstanceId.c_str(), "device instance id")
            );

            auto endpointManager = TransportState::Current().GetEndpointManager();

            if (endpointManager == nullptr)
            {
                continue;
            }

            // An endpoint which is already live is updated in place, so renaming a connected
            // device works without disconnecting it.
            auto existingEndpointDeviceId = endpointManager->FindMatchingInstantiatedEndpoint(*matchCriteria);

            if (existingEndpointDeviceId.empty())
            {
                continue;
            }

            std::vector<DEVPROPERTY> endpointDevProperties{};

            if (customProperties->WriteAllProperties(endpointDevProperties) && endpointDevProperties.size() > 0)
            {
                LOG_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
                    existingEndpointDeviceId.c_str(),
                    static_cast<ULONG>(endpointDevProperties.size()),
                    endpointDevProperties.data()));
            }
        }
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_FAIL);
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    auto responseObject = internal::BuildConfigurationResponseObject(false);

    // if we're passed a null or empty json, we just quietly exit
    if (configurationJsonSection == nullptr)
    {
        internal::JsonStringifyObjectToOutParam(responseObject, response);
        return S_OK;
    }

    json::JsonObject jsonObject;

    if (!json::JsonObject::TryParse(configurationJsonSection, jsonObject))
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Invalid Bluetooth MIDI transport configuration JSON.");
        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return S_OK;
    }

    auto commandHelper = internal::MidiTransportCommandHelper::ParseCommand(jsonObject);
    auto commandName = commandHelper.Command();

    if (commandName.empty())
    {
        // not a command, so this is the transport's own section of the configuration file
        QueueConfiguredDevices(jsonObject);
        QueueConfiguredPeripheral(jsonObject);

        if (jsonObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY))
        {
            auto const preference = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(
                jsonObject.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY, L""),
                TransportState::Current().GetConnectionParameterPreference());

            TransportState::Current().SetConnectionParameterPreference(preference);
        }

        LOG_IF_FAILED(ProcessEndpointCustomizations(jsonObject, responseObject));

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return S_OK;
    }

    if (commandName == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES)
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CREATE_WITH_IMAGE, true);

        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_LIST_AVAILABLE_DEVICES)
    {
        json::JsonArray devicesJson;

        if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            for (auto const& device : endpointManager->GetDiscoveredDevices())
            {
                devicesJson.Append(BuildAvailableDeviceJson(device));
            }
        }

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_AVAILABLE_DEVICES_RESPONSE_KEY, devicesJson);
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_CONNECT_DEVICE)
    {
        auto deviceId = GetCommandDeviceId(commandHelper);

        if (deviceId.empty())
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI connectDevice requires a deviceId.");
        }
        else if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            auto hr = endpointManager->ConnectDevice(deviceId);

            if (SUCCEEDED(hr))
            {
                // The request is remembered rather than performed, so the caller is told whether
                // the device is actually there. Otherwise a connect to a device which is powered
                // off looks exactly like one to a device sitting on the desk.
                bool isKnown{ false };
                bool isPresent{ false };
                winrt::hstring name{};

                for (auto const& device : endpointManager->GetDiscoveredDevices())
                {
                    if (device.Id == deviceId)
                    {
                        isKnown = true;
                        isPresent = device.IsPresent;
                        name = device.Name;
                        break;
                    }
                }

                responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_KNOWN_KEY, json::JsonValue::CreateBooleanValue(isKnown));
                responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PRESENT_KEY, json::JsonValue::CreateBooleanValue(isPresent));
                responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY, json::JsonValue::CreateStringValue(name));

                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
            else
            {
                SetCommandHresultFailure(responseObject, hr, L"Bluetooth MIDI connectDevice failed.");
            }
        }
        else
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI endpoint manager is not available.");
        }
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DISCONNECT_DEVICE)
    {
        auto deviceId = GetCommandDeviceId(commandHelper);

        if (deviceId.empty())
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI disconnectDevice requires a deviceId.");
        }
        else if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            auto hr = endpointManager->DisconnectDevice(deviceId);

            if (SUCCEEDED(hr))
            {
                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
            else
            {
                SetCommandHresultFailure(responseObject, hr, L"Bluetooth MIDI disconnectDevice failed.");
            }
        }
        else
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI endpoint manager is not available.");
        }
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_START_PERIPHERAL)
    {
        auto const protocol = ParseProtocolJsonString(
            GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY),
            MidiBleProtocol::Protocol::Midi1);

        if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            auto hr = endpointManager->StartPeripheral(protocol);

            if (SUCCEEDED(hr))
            {
                AddPeripheralStatusToResponse(responseObject);
                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
            else
            {
                SetCommandHresultFailure(responseObject, hr, L"Bluetooth MIDI startPeripheral failed.");
            }
        }
        else
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI endpoint manager is not available.");
        }
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_STOP_PERIPHERAL)
    {
        if (auto endpointManager = TransportState::Current().GetEndpointManager())
        {
            auto hr = endpointManager->StopPeripheral();

            if (SUCCEEDED(hr))
            {
                AddPeripheralStatusToResponse(responseObject);
                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
            else
            {
                SetCommandHresultFailure(responseObject, hr, L"Bluetooth MIDI stopPeripheral failed.");
            }
        }
        else
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Bluetooth MIDI endpoint manager is not available.");
        }
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PERIPHERAL_STATUS)
    {
        AddPeripheralStatusToResponse(responseObject);
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_SET_CONNECTION_PARAMETERS)
    {
        auto const requested = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY);

        auto const current = TransportState::Current().GetConnectionParameterPreference();
        auto const preference = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(requested, current);

        if (!requested.empty() && preference == current &&
            requested != MidiBleUtilities::ConnectionParameterPreferenceToJsonString(current))
        {
            internal::SetConfigurationResponseObjectFail(responseObject, L"Unrecognized Bluetooth MIDI connection parameter preference.");
        }
        else
        {
            TransportState::Current().SetConnectionParameterPreference(preference);

            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY,
                json::JsonValue::CreateStringValue(MidiBleUtilities::ConnectionParameterPreferenceToJsonString(preference)));

            // Reported so the caller can see what the preset actually asks for rather than
            // inferring it from the name.
            auto const parameters = MidiBleUtilities::GetPreferredConnectionParameters(preference);

            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MIN_INTERVAL_MS_KEY,
                json::JsonValue::CreateNumberValue(parameters != nullptr ? parameters.MinConnectionInterval() * 1.25 : 0.0));

            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MAX_INTERVAL_MS_KEY,
                json::JsonValue::CreateNumberValue(parameters != nullptr ? parameters.MaxConnectionInterval() * 1.25 : 0.0));

            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Unsupported Bluetooth MIDI transport command.");
    }

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, response);

    return S_OK;
}


HRESULT
CMidi2Ble2MidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );




    return S_OK;
}

