// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Reads the transport's configuration section and runs its commands.
//
// Section shape, keyed by entry identifier the way Network MIDI 2.0 keys its entries:
//
//   "create": { "hosts": { "{guid}": { ... } }, "clients": { "{guid}": { ... } } }
//   "update": [ endpoint customizations ]
//   "remove": { "update": [ customization removals ] }
//
// Sending a create for an existing identifier replaces that entry. Hosts and clients are taken
// away with the removeHost and removeClient commands, which also stop what is running.
// ============================================================================

#include "pch.h"

namespace
{
    void Fail(_Inout_ json::JsonObject& responseObject, _In_ uint32_t const errorCode, _In_ UINT const messageId)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, errorCode, internal::ResourceGetWString(messageId));
    }

    // Names are sent in AppleMIDI invitations and, for a host, advertised as a DNS-SD label
    bool IsValidName(_In_ std::wstring const& name, _In_ bool const advertised, _Out_ uint32_t& errorCode, _Out_ UINT& messageId)
    {
        errorCode = 0;
        messageId = 0;

        if (name.empty() || (advertised && name.find(L'.') != std::wstring::npos))
        {
            errorCode = RTP_MIDI_ERROR_CODE_INVALID_NAME;
            messageId = IDS_RTP_ERROR_INVALID_NAME;
            return false;
        }

        if (RtpMidiText::Utf8ByteCount(name) > MIDI_RTP_NAME_MAX_UTF8_BYTES)
        {
            errorCode = RTP_MIDI_ERROR_CODE_NAME_TOO_LONG;
            messageId = IDS_RTP_ERROR_NAME_TOO_LONG;
            return false;
        }

        return true;
    }

    bool TryReadHost(
        _In_ GUID const& entryId,
        _In_ json::JsonObject const& entry,
        _Out_ RtpMidiHostDefinition& definition,
        _Out_ uint32_t& errorCode,
        _Out_ UINT& messageId)
    {
        definition = RtpMidiHostDefinition{};
        definition.EntryId = entryId;
        definition.Name = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
        definition.ServiceInstanceName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
        definition.AllowPortFallback = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, true);
        definition.Advertise = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, true);
        definition.Enabled = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);
        definition.SendRecoveryJournal = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);

        if (!IsValidName(definition.Name, false, errorCode, messageId)) return false;
        if (definition.Advertise && !IsValidName(definition.EffectiveServiceInstanceName(), true, errorCode, messageId)) return false;

        uint16_t port{ 0 };
        bool isAuto{ false };
        bool isPresent{ false };

        if (!RtpMidiJson::TryGetPort(entry, MIDI_CONFIG_JSON_RTP_MIDI_PORT_KEY, port, isAuto, isPresent))
        {
            errorCode = RTP_MIDI_ERROR_CODE_INVALID_PORT;
            messageId = IDS_RTP_ERROR_INVALID_PORT;
            return false;
        }

        definition.Port = !isPresent ? static_cast<uint16_t>(MIDI_RTP_DEFAULT_HOST_PORT) : isAuto ? static_cast<uint16_t>(0) : port;

        return true;
    }

    bool TryReadClient(
        _In_ GUID const& entryId,
        _In_ json::JsonObject const& entry,
        _Out_ RtpMidiClientDefinition& definition,
        _Out_ uint32_t& errorCode,
        _Out_ UINT& messageId)
    {
        definition = RtpMidiClientDefinition{};
        definition.EntryId = entryId;
        definition.Name = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
        definition.RemoteServiceInstanceName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
        definition.RemoteAddress = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
        definition.CustomEndpointName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY);
        definition.AutoReconnect = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, true);
        definition.Enabled = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);
        definition.SendRecoveryJournal = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);

        if (!IsValidName(definition.Name, false, errorCode, messageId)) return false;

        // exactly one way of finding the remote
        if (definition.RemoteServiceInstanceName.empty() == definition.RemoteAddress.empty())
        {
            errorCode = RTP_MIDI_ERROR_CODE_MISSING_REMOTE;
            messageId = IDS_RTP_ERROR_MISSING_REMOTE;
            return false;
        }

        if (definition.IsDirect())
        {
            uint16_t port{ 0 };
            bool isAuto{ false };
            bool isPresent{ false };

            if (!RtpMidiJson::TryGetPort(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY, port, isAuto, isPresent) || isAuto)
            {
                errorCode = RTP_MIDI_ERROR_CODE_INVALID_PORT;
                messageId = IDS_RTP_ERROR_INVALID_PORT;
                return false;
            }

            definition.RemotePort = isPresent ? port : static_cast<uint16_t>(MIDI_RTP_DEFAULT_HOST_PORT);
        }

        return true;
    }

    // Every command that names an entry goes through this, so a bad identifier is reported the
    // same way everywhere
    bool TryGetEntryArgument(
        _In_ internal::MidiTransportCommandHelper& command,
        _Inout_ json::JsonObject& responseObject,
        _Out_ GUID& entryId)
    {
        entryId = GUID{};

        auto const arguments = command.Arguments();

        if (arguments == nullptr || arguments->find(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER) == arguments->end())
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_MISSING_ENTRY_IDENTIFIER, IDS_RTP_ERROR_MISSING_ENTRY_IDENTIFIER);
            return false;
        }

        auto const& value = arguments->at(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER);

        if (!RtpMidiJson::TryParseEntryIdentifier(internal::TrimmedWStringCopy(value), entryId))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_INVALID_ENTRY_IDENTIFIER, IDS_RTP_ERROR_INVALID_ENTRY_IDENTIFIER);
            return false;
        }

        return true;
    }
}


_Use_decl_annotations_
HRESULT
CMidi2RtpMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager)
{
    UNREFERENCED_PARAMETER(transportId);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManager);

    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
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
CMidi2RtpMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response)
try
{
    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, response);
    *response = nullptr;

    auto responseObject = internal::BuildConfigurationResponseObject(false);

    if (configurationJsonSection == nullptr)
    {
        internal::JsonStringifyObjectToOutParam(responseObject, response);
        return S_OK;
    }

    json::JsonObject jsonObject{ nullptr };
    bool parsed{ false };

    try
    {
        parsed = json::JsonObject::TryParse(configurationJsonSection, jsonObject) && jsonObject != nullptr;
    }
    catch (...)
    {
        // past its nesting limit the parser throws instead of reporting failure
        parsed = false;
    }

    if (!parsed)
    {
        Fail(responseObject, RTP_MIDI_ERROR_CODE_INVALID_JSON, IDS_RTP_ERROR_INVALID_JSON);
        internal::JsonStringifyObjectToOutParam(responseObject, response);
        return S_OK;
    }

    if (internal::MidiTransportCommandHelper::TransportObjectContainsCommand(jsonObject))
    {
        ProcessCommand(jsonObject, responseObject);
        internal::JsonStringifyObjectToOutParam(responseObject, response);
        return S_OK;
    }

    // Customizations first, so an endpoint created for a new entry below already has its name
    ProcessEndpointCustomizations(jsonObject);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    json::JsonObject createSection{ nullptr };
    if (RtpMidiJson::TryGetObject(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, createSection))
    {
        ProcessCreateSection(createSection, responseObject);
    }

    json::JsonObject removeSection{ nullptr };
    if (RtpMidiJson::TryGetObject(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, removeSection))
    {
        ProcessEndpointCustomizationRemovals(removeSection);
    }

    if (auto endpointManager = TransportState::Current().GetEndpointManager())
    {
        endpointManager->WakeWorker();
    }

    internal::JsonStringifyObjectToOutParam(responseObject, response);

    return S_OK;
}
catch (winrt::hresult_error const& ex)
{
    return ex.code();
}
catch (std::bad_alloc const&)
{
    return E_OUTOFMEMORY;
}
catch (...)
{
    // Not wil::ResultFromCaughtException: this is a COM boundary fed untrusted JSON, and WIL fail
    // fasts on an exception type it does not recognize
    return E_UNEXPECTED;
}


_Use_decl_annotations_
void
CMidi2RtpMidiConfigurationManager::ProcessCreateSection(json::JsonObject const& createSection, json::JsonObject& responseObject)
{
    // An invalid entry is skipped rather than failing the whole section, because the entries
    // before it are already in effect. The first failure is what gets reported.
    bool anyEntryFailed{ false };

    auto const reportFailure = [&](winrt::hstring const& key, uint32_t const errorCode, UINT const messageId)
        {
            TraceLoggingWrite(
                MidiRtpMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invalid configuration entry skipped", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(key.c_str(), "entry identifier"),
                TraceLoggingUInt32(errorCode, "error code")
            );

            if (!anyEntryFailed)
            {
                anyEntryFailed = true;
                Fail(responseObject, errorCode, messageId);
            }
        };

    json::JsonObject hosts{ nullptr };
    if (RtpMidiJson::TryGetObject(createSection, MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY, hosts))
    {
        for (auto const& pair : hosts)
        {
            auto const key = pair.Key();

            GUID entryId{};
            if (!RtpMidiJson::TryParseEntryIdentifier(internal::TrimmedWStringCopy(std::wstring{ key }), entryId))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY_IDENTIFIER, IDS_RTP_ERROR_INVALID_ENTRY_IDENTIFIER);
                continue;
            }

            json::JsonObject entry{ nullptr };
            if (!RtpMidiJson::TryGetObject(hosts, key, entry))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY, IDS_RTP_ERROR_INVALID_ENTRY);
                continue;
            }

            RtpMidiHostDefinition definition{};
            uint32_t errorCode{ 0 };
            UINT messageId{ 0 };

            if (!TryReadHost(entryId, entry, definition, errorCode, messageId))
            {
                reportFailure(key, errorCode, messageId);
                continue;
            }

            TransportState::Current().SetHostDefinition(definition);
        }
    }

    json::JsonObject clients{ nullptr };
    if (RtpMidiJson::TryGetObject(createSection, MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, clients))
    {
        for (auto const& pair : clients)
        {
            auto const key = pair.Key();

            GUID entryId{};
            if (!RtpMidiJson::TryParseEntryIdentifier(internal::TrimmedWStringCopy(std::wstring{ key }), entryId))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY_IDENTIFIER, IDS_RTP_ERROR_INVALID_ENTRY_IDENTIFIER);
                continue;
            }

            json::JsonObject entry{ nullptr };
            if (!RtpMidiJson::TryGetObject(clients, key, entry))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY, IDS_RTP_ERROR_INVALID_ENTRY);
                continue;
            }

            RtpMidiClientDefinition definition{};
            uint32_t errorCode{ 0 };
            UINT messageId{ 0 };

            if (!TryReadClient(entryId, entry, definition, errorCode, messageId))
            {
                reportFailure(key, errorCode, messageId);
                continue;
            }

            TransportState::Current().SetClientDefinition(definition);
        }
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiConfigurationManager::ProcessEndpointCustomizations(json::JsonObject const& section)
{
    json::JsonArray updates{ nullptr };
    if (!RtpMidiJson::TryGetArray(section, MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, updates)) return;

    for (uint32_t i = 0; i < updates.Size(); i++)
    {
        try
        {
            auto const element = updates.GetAt(i);
            if (element == nullptr || element.ValueType() != json::JsonValueType::Object) continue;

            auto const update = element.GetObject();

            json::JsonObject matchObject{ nullptr };
            json::JsonObject propertiesObject{ nullptr };

            if (!RtpMidiJson::TryGetObject(update, WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey, matchObject)) continue;
            if (!RtpMidiJson::TryGetObject(update, WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey, propertiesObject)) continue;

            auto matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);

            // An image is a bare file name. A path would let the configuration file point the
            // service at an arbitrary location.
            auto customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJsonRejectingImagePath(propertiesObject);

            if (matchCriteria == nullptr || customProperties == nullptr) continue;

            // Cached whether or not the endpoint exists yet: an rtpMIDI endpoint only appears
            // when a remote connects, usually long after the configuration is read
            LOG_HR_IF(E_FAIL, !m_customPropertiesCache->Add(matchCriteria, customProperties));

            auto endpointManager = TransportState::Current().GetEndpointManager();
            if (endpointManager == nullptr || m_midiDeviceManager == nullptr) continue;

            auto const existing = endpointManager->FindMatchingInstantiatedEndpoint(*matchCriteria);
            if (existing.empty()) continue;

            std::vector<DEVPROPERTY> properties{};

            if (customProperties->WriteAllProperties(properties) && !properties.empty())
            {
                LOG_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
                    existing.c_str(),
                    static_cast<ULONG>(properties.size()),
                    properties.data()));
            }
        }
        CATCH_LOG();
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiConfigurationManager::ProcessEndpointCustomizationRemovals(json::JsonObject const& removeSection)
{
    json::JsonArray removals{ nullptr };
    if (!RtpMidiJson::TryGetArray(removeSection, MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, removals)) return;

    for (uint32_t i = 0; i < removals.Size(); i++)
    {
        try
        {
            auto const element = removals.GetAt(i);
            if (element == nullptr || element.ValueType() != json::JsonValueType::Object) continue;

            json::JsonObject matchObject{ nullptr };
            if (!RtpMidiJson::TryGetObject(element.GetObject(), WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey, matchObject)) continue;

            auto matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);
            if (matchCriteria == nullptr) continue;

            // takes effect the next time the endpoint is created
            m_customPropertiesCache->Remove(*matchCriteria);
        }
        CATCH_LOG();
    }
}


_Use_decl_annotations_
void
CMidi2RtpMidiConfigurationManager::ProcessCommand(json::JsonObject const& section, json::JsonObject& responseObject)
{
    auto command = internal::MidiTransportCommandHelper::ParseCommand(section);
    auto const verb = command.Command();

    auto endpointManager = TransportState::Current().GetEndpointManager();

    if (verb == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES)
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CREATE_WITH_IMAGE, true);

        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE, true);

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);
        return;
    }

    if (endpointManager == nullptr || !endpointManager->IsInitialized())
    {
        Fail(responseObject, RTP_MIDI_ERROR_CODE_NOT_READY, IDS_RTP_ERROR_NOT_READY);
        return;
    }

    if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS)
    {
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY, endpointManager->BuildHostsStatusJson());
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS)
    {
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY, endpointManager->BuildClientsStatusJson());
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED)
    {
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_PEERS_KEY, endpointManager->BuildAdvertisedPeersJson());
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST || verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST)
    {
        GUID entryId{};
        if (!TryGetEntryArgument(command, responseObject, entryId)) return;

        if (!TransportState::Current().SetHostEnabled(entryId, verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        endpointManager->WakeWorker();
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST || verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT)
    {
        GUID entryId{};
        if (!TryGetEntryArgument(command, responseObject, entryId)) return;

        bool const removed = verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST ?
            TransportState::Current().RemoveHostDefinition(entryId) :
            TransportState::Current().RemoveClientDefinition(entryId);

        if (!removed)
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        // the worker says goodbye to the remotes and removes the endpoints
        endpointManager->WakeWorker();
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT)
    {
        GUID entryId{};
        if (!TryGetEntryArgument(command, responseObject, entryId)) return;

        if (FAILED(endpointManager->ReconnectClient(entryId)))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE)
    {
        GUID entryId{};
        if (!TryGetEntryArgument(command, responseObject, entryId)) return;

        auto const arguments = command.Arguments();
        auto const connectionArgument = arguments->find(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_CONNECTION_ID);

        uint32_t connectionId{ 0 };
        bool haveConnectionId{ false };

        if (connectionArgument != arguments->end())
        {
            auto const text = internal::TrimmedWStringCopy(connectionArgument->second);

            if (!text.empty() && text.size() <= 10 && std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
            {
                auto const value = std::stoull(text);

                if (value > 0 && value <= UINT32_MAX)
                {
                    connectionId = static_cast<uint32_t>(value);
                    haveConnectionId = true;
                }
            }
        }

        if (!haveConnectionId)
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_MISSING_CONNECTION_ID, IDS_RTP_ERROR_MISSING_CONNECTION_ID);
            return;
        }

        if (FAILED(endpointManager->DisconnectConnection(entryId, connectionId)))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else
    {
        Fail(responseObject, RTP_MIDI_ERROR_CODE_UNRECOGNIZED_COMMAND, IDS_RTP_ERROR_UNRECOGNIZED_COMMAND);
    }
}


HRESULT
CMidi2RtpMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiRtpMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_midiDeviceManager.reset();

    return S_OK;
}
