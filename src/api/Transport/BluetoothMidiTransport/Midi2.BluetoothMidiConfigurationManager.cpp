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
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_CODE_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.LastConnectErrorCode)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.MessagesReceived)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.MessagesSent)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEND_ERROR_HRESULT_KEY, json::JsonValue::CreateNumberValue(device.LastSendErrorHresult));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PRESENT_KEY, json::JsonValue::CreateBooleanValue(device.IsPresent));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEEN_AGO_MS_KEY, json::JsonValue::CreateNumberValue(static_cast<double>(device.LastSeenAgoMilliseconds)));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_BEEN_SEEN_KEY, json::JsonValue::CreateBooleanValue(device.HasBeenSeen));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_ENDPOINT_KEY, json::JsonValue::CreateBooleanValue(device.HasEndpoint));
        deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY, json::JsonValue::CreateNumberValue(device.ConnectionIntervalUnits * 1.25));

        // Both are reported: the first is what this device is set to, which may be "default", and
        // the second is what that actually resolves to, so a caller can show either without
        // having to know the transport setting.
        deviceJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY,
            json::JsonValue::CreateStringValue(
                MidiBleUtilities::OfflineRetentionToJsonString(
                    TransportState::Current().GetDeviceOfflineRetentionSeconds(device.Id))));

        deviceJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_EFFECTIVE_OFFLINE_RETENTION_KEY,
            json::JsonValue::CreateStringValue(
                MidiBleUtilities::OfflineRetentionToJsonString(
                    TransportState::Current().GetEffectiveOfflineRetentionSeconds(device.Id))));

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

    // ISO 8601 UTC keeping the full 100ns FILETIME resolution, which is the same wire format the
    // Network MIDI 2.0 transport uses for a pending request time.
    winrt::hstring FileTimeToIso8601(_In_ uint64_t const fileTime)
    {
        if (fileTime == 0)
        {
            return L"";
        }

        FILETIME ft{};
        ft.dwLowDateTime = static_cast<DWORD>(fileTime & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>(fileTime >> 32);

        SYSTEMTIME st{};

        if (!FileTimeToSystemTime(&ft, &st))
        {
            return L"";
        }

        wchar_t buffer[64]{};

        swprintf_s(
            buffer,
            ARRAYSIZE(buffer),
            L"%04u-%02u-%02uT%02u:%02u:%02u.%07lluZ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            fileTime % 10000000ull);

        return winrt::hstring{ buffer };
    }

    json::JsonObject BuildPendingClientJson(_In_ MidiBleProtocol::PendingPeripheralClient const& client)
    {
        json::JsonObject clientJson;

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
            json::JsonValue::CreateStringValue(client.Name));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(client.Address));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_TYPE_KEY,
            json::JsonValue::CreateStringValue(client.AddressType));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_BLUETOOTH_DEVICE_ID_KEY,
            json::JsonValue::CreateStringValue(client.BluetoothDeviceId));

        // A Central which is not bonded rotates its address, so an "always" decision about it
        // cannot be relied on to match later. The caller needs this to say so.
        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY,
            json::JsonValue::CreateBooleanValue(client.IsPaired));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_GENERIC_NAME_KEY,
            json::JsonValue::CreateBooleanValue(client.HasGenericName));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_REMEMBERABLE_KEY,
            json::JsonValue::CreateBooleanValue(client.IsRememberable));

        clientJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_REQUEST_TIME_KEY,
            json::JsonValue::CreateStringValue(FileTimeToIso8601(client.RequestedFileTime)));

        return clientJson;
    }

    void AddPendingClientsToResponse(_In_ json::JsonObject& responseObject)
    {
        json::JsonArray pendingJson;

        MidiBleProtocol::PendingPeripheralClient pending{};

        if (TransportState::Current().TryGetPendingPeripheralClient(pending))
        {
            pendingJson.Append(BuildPendingClientJson(pending));
        }

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PENDING_CLIENTS_RESPONSE_KEY, pendingJson);
    }

    // The remembered lists are reported in the same shape the configuration file stores them, so
    // a caller persisting an "always" decision can write back what it is given without having to
    // merge anything itself.
    json::JsonArray BuildRememberedClientArray(_In_ bool const allowed)
    {
        json::JsonArray entries;

        for (auto const& identity : TransportState::Current().GetRememberedPeripheralClients(allowed))
        {
            json::JsonObject entry;

            entry.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY,
                json::JsonValue::CreateStringValue(identity.Address));

            entry.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
                json::JsonValue::CreateStringValue(identity.Name));

            entries.Append(entry);
        }

        return entries;
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

        // Reported whether or not the peripheral is running, because it describes what will happen
        // to the next Central rather than the state of the current one.
        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_KEY,
            json::JsonValue::CreateStringValue(
                MidiBleUtilities::PeripheralClientPolicyToJsonString(
                    TransportState::Current().GetPeripheralClientPolicy())));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ALLOWED_CLIENTS_KEY,
            BuildRememberedClientArray(true));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DENIED_CLIENTS_KEY,
            BuildRememberedClientArray(false));

        // A remote Central being subscribed is the only sign that data can actually move.
        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY,
            json::JsonValue::CreateBooleanValue(isRunning && peripheral->IsClientSubscribed()));

        winrt::hstring endpointDeviceId{};
        winrt::hstring endpointDeviceInstanceId{};
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
                endpointDeviceInstanceId = winrt::hstring{ connection->EndpointDeviceInstanceId() };
                connectedDeviceName = connection->DeviceName();
                messagesReceived = connection->MessagesReceived();
                messagesSent = connection->MessagesSent();
            }
        }

        auto const remoteClient = isRunning ? peripheral->RemoteClientInfo() : MidiBleRemoteClientInfo{};

        // The name normally comes from the connection, but a Central waiting for approval has no
        // connection. Falling back keeps the caller from reporting a subscribed device as nameless.
        if (connectedDeviceName.empty())
        {
            connectedDeviceName = remoteClient.Name;
        }

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
            json::JsonValue::CreateStringValue(connectedDeviceName));

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

        // A customization matches on the instance id, and this is the only place it is exposed
        // for the peripheral endpoint. It is empty until a Central connects, because the id is
        // built from the remote device's identity.
        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY,
            json::JsonValue::CreateStringValue(endpointDeviceInstanceId));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(messagesReceived)));

        peripheralJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(messagesSent)));

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, peripheralJson);
    }

    void AddRadioCapabilitiesToResponse(_In_ json::JsonObject& responseObject)
    {
        auto const capabilities = TransportState::Current().GetRadioCapabilities();

        json::JsonObject radioJson;

        radioJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PRESENT_KEY,
            json::JsonValue::CreateBooleanValue(capabilities.RadioPresent));

        radioJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_LOW_ENERGY_KEY,
            json::JsonValue::CreateBooleanValue(capabilities.LowEnergySupported));

        radioJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_CENTRAL_ROLE_KEY,
            json::JsonValue::CreateBooleanValue(capabilities.CentralRoleSupported));

        radioJson.SetNamedValue(
            MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PERIPHERAL_ROLE_KEY,
            json::JsonValue::CreateBooleanValue(capabilities.PeripheralRoleSupported));

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_KEY, radioJson);
    }

    void SetCommandHresultFailure(
        _In_ json::JsonObject& responseObject,
        _In_ HRESULT hr,
        _In_ uint32_t errorCode,
        _In_ std::wstring const& message)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, errorCode, message);
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_RESULT_HRESULT_KEY, json::JsonValue::CreateNumberValue(static_cast<int32_t>(hr)));
    }

    // Publishing this PC fails for reasons a customer can act on, so they are told apart here
    // rather than being collapsed into one HRESULT.
    uint32_t PeripheralCommandErrorCode(_In_ HRESULT const hr, _In_ bool const isStopping)
    {
        if (hr == E_INVALIDARG)                                     return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_INVALID_PROTOCOL;
        if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED))    return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ALREADY_RUNNING;
        if (hr == HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE))     return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ROLE_NOT_AVAILABLE;
        if (hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED))   return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_NO_CLIENT;
        if (hr == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))          return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ROLE_NOT_AVAILABLE;
        if (hr == E_UNEXPECTED && isStopping)                       return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_NOT_RUNNING;

        return BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ADVERTISING_FAILED;
    }

    // The configuration file lists the devices the user has already approved, so they are
    // reconnected without any app running. Discovery has to find each one first, so this only
    // queues the request; the endpoint manager connects when the device is seen.
    void QueueConfiguredDevices(_In_ json::JsonObject const& transportObject)
    {
        json::JsonArray devicesArray{ nullptr };

        if (!MidiBleProtocol::SafeJson::TryGetArray(transportObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY, devicesArray))
        {
            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingWideString(L"No devices array in this configuration section, so nothing was queued for reconnect", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(transportObject.Stringify().c_str(), "transport section")
            );

            return;
        }

        auto endpointManager = TransportState::Current().GetEndpointManager();

        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"Reading the configured Bluetooth MIDI devices", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(devicesArray.Size(), "device count"),
            TraceLoggingBool(endpointManager != nullptr, "endpoint manager available"),
            TraceLoggingBool(endpointManager != nullptr && endpointManager->IsInitialized(), "endpoint manager initialized")
        );

        for (auto const& entry : devicesArray)
        {
            // Iterating a JsonArray yields IJsonValue, which does not cast to JsonObject. It has
            // to be asked for its object, the same way the remembered client list does it.
            if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingWideString(L"Skipping a configured Bluetooth MIDI device which is not a JSON object", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                continue;
            }

            auto const deviceObject = entry.GetObject();

            if (!MidiBleProtocol::SafeJson::GetBoolean(deviceObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY, true))
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingWideString(L"Skipping a configured Bluetooth MIDI device which is disabled", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceObject.Stringify().c_str(), "device")
                );

                continue;
            }

            auto const deviceId = MidiBleProtocol::SafeJson::GetString(deviceObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY);

            // Every command keys on a 12 hex digit address, so anything else in the file is a
            // typo or tampering and would otherwise sit in the connect list forever.
            if (!MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring{ deviceId }))
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingWideString(L"Skipping a configured Bluetooth MIDI device with an unusable id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceId.c_str(), "device id")
                );

                continue;
            }

            // Parked either way. The endpoint manager may not exist yet, and if it does it
            // drains this list again on the way up, so the id can never be dropped.
            TransportState::Current().AddConfiguredDeviceId(deviceId);

            if (deviceObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY))
            {
                int32_t retentionSeconds{ MidiBleProtocol::OfflineRetentionUseTransportDefault };

                if (MidiBleUtilities::TryOfflineRetentionFromJsonString(
                    MidiBleProtocol::SafeJson::GetString(deviceObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY),
                    true,
                    retentionSeconds))
                {
                    TransportState::Current().SetDeviceOfflineRetentionSeconds(deviceId, retentionSeconds);
                }
                else
                {
                    TraceLoggingWrite(
                        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingWideString(L"Ignoring an unusable offline retention for a configured Bluetooth MIDI device", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(deviceId.c_str(), "device id")
                    );
                }
            }

            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingWideString(L"Parked a configured Bluetooth MIDI device for reconnect", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(deviceId.c_str(), "device id"),
                TraceLoggingBool(endpointManager != nullptr && endpointManager->IsInitialized(), "connecting now")
            );

            if (endpointManager != nullptr && endpointManager->IsInitialized())
            {
                LOG_IF_FAILED(endpointManager->ConnectConfiguredDevices());
            }
        }
    }

    // Publishing this PC as a peripheral is off unless the configuration file asks for it, because
    // it makes the machine visible and connectable to anything nearby.
    // An entry with no usable address is dropped rather than kept, because it could never match a
    // Central and would only make the remembered list look like it says more than it does.
    std::vector<MidiBleProtocol::PeripheralClientIdentity> ReadClientIdentityList(
        _In_ json::JsonObject const& peripheralObject,
        _In_ std::wstring const& key)
    {
        std::vector<MidiBleProtocol::PeripheralClientIdentity> results{};

        json::JsonArray entries{ nullptr };

        if (!MidiBleProtocol::SafeJson::TryGetArray(peripheralObject, key, entries))
        {
            return results;
        }

        for (auto const& entry : entries)
        {
            if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
            {
                continue;
            }

            auto const entryObject = entry.GetObject();

            MidiBleProtocol::PeripheralClientIdentity identity{};

            identity.Address = MidiBleProtocol::SafeJson::GetString(
                entryObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY).c_str();

            identity.Name = MidiBleProtocol::SafeJson::GetString(
                entryObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY).c_str();

            if (!MidiBleUtilities::IsWellFormedBluetoothDeviceId(identity.Address))
            {
                continue;
            }

            results.push_back(identity);
        }

        return results;
    }

    void QueueConfiguredPeripheral(_In_ json::JsonObject const& transportObject)    {
        if (transportObject == nullptr)
        {
            return;
        }

        json::JsonObject peripheralObject{ nullptr };

        if (!MidiBleProtocol::SafeJson::TryGetObject(transportObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, peripheralObject))
        {
            return;
        }

        // Read before the enabled check, because the answer has to be ready for whenever the
        // peripheral is started, which may be by a command long after this file was read.
        TransportState::Current().SetPeripheralClientPolicy(
            MidiBleUtilities::PeripheralClientPolicyFromJsonString(
                MidiBleProtocol::SafeJson::GetString(peripheralObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_KEY)));

        TransportState::Current().SetRememberedPeripheralClients(
            ReadClientIdentityList(peripheralObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ALLOWED_CLIENTS_KEY),
            ReadClientIdentityList(peripheralObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DENIED_CLIENTS_KEY));

        if (!MidiBleProtocol::SafeJson::GetBoolean(peripheralObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ENABLED_KEY, false))
        {
            return;
        }

        // MIDI 1.0 by default, because nothing on the market speaks BLE MIDI 2.0 yet and only one
        // characteristic can be published at a time.
        auto const protocol = ParseProtocolJsonString(
            MidiBleProtocol::SafeJson::GetString(peripheralObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY),
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
CMidi2BluetoothMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager
)
{
    UNREFERENCED_PARAMETER(transportId);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManager);


    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
CMidi2BluetoothMidiConfigurationManager::ProcessEndpointCustomizations(
    json::JsonObject const& jsonObject,
    json::JsonObject& responseObject) noexcept
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        UNREFERENCED_PARAMETER(responseObject);

        try
        {
            json::JsonArray updateArray{ nullptr };

            if (!MidiBleProtocol::SafeJson::TryGetArray(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, updateArray) ||
                updateArray.Size() == 0)
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

                json::JsonObject matchObject{ nullptr };

                if (!MidiBleProtocol::SafeJson::TryGetObject(
                        updateObject,
                        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey,
                        matchObject))
                {
                    // nothing to tie this customization to
                    continue;
                }

                json::JsonObject customPropertiesObject{ nullptr };

                if (!MidiBleProtocol::SafeJson::TryGetObject(
                        updateObject,
                        WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey,
                        customPropertiesObject))
                {
                    continue;
                }

                auto matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);

                // An image is a bare file name. A path here would let a configuration file point the
                // service at an arbitrary location.
                auto customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
                    customPropertiesObject);

                if (matchCriteria == nullptr || customProperties == nullptr)
                {
                    continue;
                }

                // Cached whether or not the endpoint exists yet. A Bluetooth endpoint is created only
                // once the device is in range and answers, which is usually long after this arrives,
                // and the creation path reads this cache before it activates the device node.
                LOG_HR_IF(E_FAIL, !m_customPropertiesCache->Add(matchCriteria, customProperties));

                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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

                // The MIDI 1.0 ports are named from the group terminal blocks and the port name
                // table, neither of which the customization touches, so a rename would otherwise
                // stop at the endpoint node and leave the WinMM ports on the old name.
                LOG_IF_FAILED(endpointManager->RefreshMidi1PortsForRenamedEndpoint(
                    existingEndpointDeviceId,
                    customProperties));
            }
        }
        catch (...)
        {
            RETURN_IF_FAILED(E_FAIL);
        }

        return S_OK;
    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
            internal::SetConfigurationResponseObjectFailWithErrorCode(
                responseObject,
                BLUETOOTH_MIDI_ERROR_CODE_INVALID_JSON,
                L"Invalid Bluetooth MIDI transport configuration JSON.");
            internal::JsonStringifyObjectToOutParam(responseObject, response);

            return S_OK;
        }

        auto commandHelper = internal::MidiTransportCommandHelper::ParseCommand(jsonObject);
        auto commandName = commandHelper.Command();

        if (commandName.empty())
        {
            // not a command, so this is the transport's own section of the configuration file
            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Processing the transport's own configuration file section", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingBool(jsonObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY), "has devices key"),
                TraceLoggingBool(jsonObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY), "has peripheral key")
            );

            QueueConfiguredDevices(jsonObject);
            QueueConfiguredPeripheral(jsonObject);

            if (jsonObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY))
            {
                auto const preference = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(
                    MidiBleProtocol::SafeJson::GetString(jsonObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY),
                    TransportState::Current().GetConnectionParameterPreference());

                TransportState::Current().SetConnectionParameterPreference(preference);
            }

            // Order against the devices array does not matter: retention is only ever evaluated by the
            // background sweep, long after the whole section has been read.
            if (jsonObject.HasKey(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY))
            {
                int32_t retentionSeconds{ MidiBleProtocol::OfflineRetentionKeepAlways };

                if (MidiBleUtilities::TryOfflineRetentionFromJsonString(
                    MidiBleProtocol::SafeJson::GetString(jsonObject, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY),
                    false,
                    retentionSeconds))
                {
                    TransportState::Current().SetDefaultOfflineRetentionSeconds(retentionSeconds);
                }
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

            // The transport-wide default, so a caller can show it without having to find a device
            // which happens to be deferring to it.
            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY,
                json::JsonValue::CreateStringValue(
                    MidiBleUtilities::OfflineRetentionToJsonString(
                        TransportState::Current().GetDefaultOfflineRetentionSeconds())));

            AddRadioCapabilitiesToResponse(responseObject);
            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_CONNECT_DEVICE)
        {
            auto deviceId = GetCommandDeviceId(commandHelper);

            if (deviceId.empty())
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_MISSING_DEVICE_ID,
                    L"Bluetooth MIDI connectDevice requires a deviceId.");
            }
            else if (!MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring{ deviceId }))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_DEVICE_ID,
                    L"A Bluetooth MIDI deviceId must be a 12 hex digit Bluetooth address.");
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
                    SetCommandHresultFailure(
                        responseObject,
                        hr,
                        hr == E_INVALIDARG
                            ? BLUETOOTH_MIDI_ERROR_CODE_INVALID_DEVICE_ID
                            : BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE,
                        L"Bluetooth MIDI connectDevice failed.");
                }
            }
            else
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE,
                    L"Bluetooth MIDI endpoint manager is not available.");
            }
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DISCONNECT_DEVICE)
        {
            auto deviceId = GetCommandDeviceId(commandHelper);

            if (deviceId.empty())
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_MISSING_DEVICE_ID,
                    L"Bluetooth MIDI disconnectDevice requires a deviceId.");
            }
            else if (!MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring{ deviceId }))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_DEVICE_ID,
                    L"A Bluetooth MIDI deviceId must be a 12 hex digit Bluetooth address.");
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
                    SetCommandHresultFailure(
                        responseObject,
                        hr,
                        hr == E_NOTFOUND
                            ? BLUETOOTH_MIDI_ERROR_CODE_DEVICE_NOT_DISCOVERED
                            : BLUETOOTH_MIDI_ERROR_CODE_NOT_CONNECTED,
                        L"Bluetooth MIDI disconnectDevice failed.");
                }
            }
            else
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE,
                    L"Bluetooth MIDI endpoint manager is not available.");
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
                    SetCommandHresultFailure(
                        responseObject,
                        hr,
                        PeripheralCommandErrorCode(hr, false),
                        L"Bluetooth MIDI startPeripheral failed.");
                }
            }
            else
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE,
                    L"Bluetooth MIDI endpoint manager is not available.");
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
                    SetCommandHresultFailure(
                        responseObject,
                        hr,
                        PeripheralCommandErrorCode(hr, true),
                        L"Bluetooth MIDI stopPeripheral failed.");
                }
            }
            else
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE,
                    L"Bluetooth MIDI endpoint manager is not available.");
            }
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PERIPHERAL_STATUS)
        {
            AddPeripheralStatusToResponse(responseObject);
            AddPendingClientsToResponse(responseObject);
            AddRadioCapabilitiesToResponse(responseObject);
            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PENDING_CLIENTS)
        {
            AddPendingClientsToResponse(responseObject);
            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_APPROVE_CLIENT ||
                 commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DENY_CLIENT)
        {
            auto const approve = (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_APPROVE_CLIENT);
            auto const address = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY);

            MidiBleProtocol::ApprovalScope scope{ MidiBleProtocol::ApprovalScope::Once };

            if (address.empty())
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_MISSING_CLIENT_ADDRESS,
                    L"A Bluetooth address is required to identify the client being decided about.");
            }
            else if (!MidiBleUtilities::TryApprovalScopeFromJsonString(
                GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_KEY), scope))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_APPROVAL_SCOPE,
                    L"Unrecognized approval scope. Use once, untilRestart or always.");
            }
            else
            {
                bool shouldPersist{ false };
                MidiBleProtocol::PeripheralClientIdentity identity{};

                auto const decisionError = TransportState::Current().ApplyPeripheralClientDecision(
                    std::wstring{ address }, approve, scope, shouldPersist, identity);

                if (decisionError == BLUETOOTH_MIDI_ERROR_CODE_ADDRESS_NOT_REMEMBERABLE)
                {
                    internal::SetConfigurationResponseObjectFailWithErrorCode(
                        responseObject,
                        decisionError,
                        L"This device's Bluetooth address changes periodically, so a permanent decision "
                        L"about it cannot be honored. Pair the device first, or choose a scope of once "
                        L"or untilRestart.");
                }
                else if (decisionError == BLUETOOTH_MIDI_ERROR_CODE_CLIENT_IDENTITY_MISMATCH)
                {
                    internal::SetConfigurationResponseObjectFailWithErrorCode(
                        responseObject,
                        decisionError,
                        L"A different Bluetooth MIDI client is waiting for a decision.");
                }
                else if (decisionError != 0)
                {
                    internal::SetConfigurationResponseObjectFailWithErrorCode(
                        responseObject,
                        decisionError,
                        L"No Bluetooth MIDI client is waiting for a decision.");
                }
                else
                {
                    // Approving has to create the endpoint, and denying has to make sure one is not
                    // left behind, so both re-run the peripheral client check.
                    if (auto endpointManager = TransportState::Current().GetEndpointManager())
                    {
                        endpointManager->OnPeripheralClientChanged();
                    }

                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_KEY,
                        json::JsonValue::CreateStringValue(
                            MidiBleUtilities::PeripheralClientDecisionToJsonString(
                                approve ?
                                    MidiBleProtocol::PeripheralClientDecision::Allowed :
                                    MidiBleProtocol::PeripheralClientDecision::Denied)));

                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_KEY,
                        json::JsonValue::CreateStringValue(MidiBleUtilities::ApprovalScopeToJsonString(scope)));

                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY,
                        json::JsonValue::CreateStringValue(identity.Address));

                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
                        json::JsonValue::CreateStringValue(identity.Name));

                    // The service applies every scope immediately but never writes the configuration
                    // file, so "always" only survives a restart if the caller records it.
                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERSIST_REQUIRED_KEY,
                        json::JsonValue::CreateBooleanValue(shouldPersist));

                    internal::SetConfigurationResponseObjectSuccess(responseObject);
                }
            }
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_FORGET_CLIENT)
        {
            auto const address = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY);

            if (address.empty())
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_MISSING_CLIENT_ADDRESS,
                    L"A Bluetooth address is required to identify the client to forget.");
            }
            else if (!TransportState::Current().ForgetPeripheralClient(std::wstring{ address }))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_CLIENT_NOT_REMEMBERED,
                    L"No remembered Bluetooth MIDI client has that address.");
            }
            else
            {
                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERSIST_REQUIRED_KEY,
                    json::JsonValue::CreateBooleanValue(true));

                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_SET_OFFLINE_RETENTION)
        {
            // No device id means the transport default, which is what a device set to "default" uses.
            auto const deviceId = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY);
            auto const requested = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY);

            int32_t retentionSeconds{ MidiBleProtocol::OfflineRetentionKeepAlways };

            if (!MidiBleUtilities::TryOfflineRetentionFromJsonString(requested, !deviceId.empty(), retentionSeconds))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_OFFLINE_RETENTION,
                    L"Offline retention must be \"always\", \"immediate\", a whole number of seconds up to 86400, or \"default\" for a single device.");
            }
            else if (!deviceId.empty() && !MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring{ deviceId }))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_DEVICE_ID,
                    L"That is not a usable Bluetooth device id.");
            }
            else
            {
                if (deviceId.empty())
                {
                    TransportState::Current().SetDefaultOfflineRetentionSeconds(retentionSeconds);
                }
                else
                {
                    TransportState::Current().SetDeviceOfflineRetentionSeconds(deviceId, retentionSeconds);
                }

                // Applied immediately: a device already offline should not have to wait for another
                // drop before a shortened retention takes effect.
                if (auto endpointManager = TransportState::Current().GetEndpointManager())
                {
                    LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());
                }

                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY,
                    json::JsonValue::CreateStringValue(MidiBleUtilities::OfflineRetentionToJsonString(retentionSeconds)));

                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERSIST_REQUIRED_KEY,
                    json::JsonValue::CreateBooleanValue(true));

                internal::SetConfigurationResponseObjectSuccess(responseObject);
            }
        }
        else if (commandName == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_SET_CONNECTION_PARAMETERS)
        {
            auto const requested = GetCommandArgument(commandHelper, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY);

            auto const current = TransportState::Current().GetConnectionParameterPreference();
            auto const preference = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(requested, current);

            if (!requested.empty() && preference == current &&
                requested != MidiBleUtilities::ConnectionParameterPreferenceToJsonString(current))
            {
                internal::SetConfigurationResponseObjectFailWithErrorCode(
                    responseObject,
                    BLUETOOTH_MIDI_ERROR_CODE_INVALID_JSON,
                    L"Unrecognized Bluetooth MIDI connection parameter preference.");
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
            internal::SetConfigurationResponseObjectFailWithErrorCode(
                responseObject,
                BLUETOOTH_MIDI_ERROR_CODE_UNRECOGNIZED_COMMAND,
                L"Unsupported Bluetooth MIDI transport command.");
        }

        // return the json with the information the client will need
        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return S_OK;
    }
    CATCH_RETURN()
}


HRESULT
CMidi2BluetoothMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );




    return S_OK;
}

