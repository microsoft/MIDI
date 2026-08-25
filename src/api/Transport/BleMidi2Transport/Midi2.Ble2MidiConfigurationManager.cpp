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

        return deviceJson;
    }

    winrt::hstring GetCommandDeviceId(_In_ internal::MidiTransportCommandHelper& commandHelper)
    {
        auto arg = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY);

        if (arg != commandHelper.Arguments()->end())
        {
            return winrt::hstring{ arg->second };
        }

        return L"";
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

        if (endpointManager == nullptr)
        {
            return;
        }

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

            if (!deviceId.empty())
            {
                LOG_IF_FAILED(endpointManager->ConnectDevice(deviceId));
            }
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

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return S_OK;
    }

    if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_LIST_AVAILABLE_DEVICES)
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

