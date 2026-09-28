// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Reads the transport's configuration section and runs its commands.
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

    // What a host or client is called when the customer gave it no name, as macOS does
    std::wstring ThisPcName()
    {
        wchar_t buffer[256]{};
        DWORD size{ ARRAYSIZE(buffer) };

        if (GetComputerNameExW(ComputerNameDnsHostname, buffer, &size) && size > 0) return std::wstring{ buffer, size };

        size = ARRAYSIZE(buffer);
        if (GetComputerNameExW(ComputerNameNetBIOS, buffer, &size) && size > 0) return std::wstring{ buffer, size };

        return {};
    }

    // A missing or empty name means this PC's name. A name which is not a string is refused.
    bool TryReadName(_In_ json::JsonObject const& entry, _Out_ std::wstring& name, _Out_ uint32_t& errorCode, _Out_ UINT& messageId)
    {
        name.clear();
        errorCode = 0;
        messageId = 0;

        json::IJsonValue value{ nullptr };
        bool present{ false };

        try
        {
            present = entry != nullptr && entry.HasKey(MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
        }
        catch (...)
        {
            present = false;
        }

        if (present && !RtpMidiJson::TryGetValue(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY, json::JsonValueType::String, value))
        {
            errorCode = RTP_MIDI_ERROR_CODE_INVALID_NAME;
            messageId = IDS_RTP_ERROR_INVALID_NAME;
            return false;
        }

        name = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY);
        if (name.empty()) name = ThisPcName();

        return true;
    }

    // Missing means anyone may connect, as in Network MIDI 2.0. Anything present but not allowAny
    // means approval, so a damaged entry never opens a host up.
    RtpMidiRemoteClientPolicy ReadRemoteClientPolicy(_In_ json::JsonObject const& entry)
    {
        bool present{ false };

        try
        {
            present = entry != nullptr && entry.HasKey(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY);
        }
        catch (...)
        {
            return RtpMidiRemoteClientPolicy::RequireApproval;
        }

        if (!present) return RtpMidiRemoteClientPolicy::AllowAny;

        auto const value = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY);

        return _wcsicmp(value.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY) == 0 ?
            RtpMidiRemoteClientPolicy::AllowAny :
            RtpMidiRemoteClientPolicy::RequireApproval;
    }

    // Entries which are not objects with a remote name are skipped
    std::vector<std::wstring> ReadRemoteClientNames(_In_ json::JsonObject const& entry, _In_ std::wstring_view const key)
    {
        std::vector<std::wstring> names;

        json::JsonArray list{ nullptr };
        if (!RtpMidiJson::TryGetArray(entry, key, list)) return names;

        for (uint32_t i = 0; i < list.Size() && names.size() < MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST; i++)
        {
            try
            {
                auto const element = list.GetAt(i);
                if (element == nullptr || element.ValueType() != json::JsonValueType::Object) continue;

                auto name = RtpMidiJson::GetString(element.GetObject(), MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);
                if (!name.empty()) names.push_back(std::move(name));
            }
            CATCH_LOG();
        }

        return names;
    }

    // ISO 8601 UTC with the full FILETIME precision, for example 2026-09-27T22:14:05.1234567Z
    std::wstring ToIso8601(_In_ FILETIME const& time)
    {
        SYSTEMTIME utc{};
        if (!FileTimeToSystemTime(&time, &utc)) return {};

        ULARGE_INTEGER ticks{};
        ticks.LowPart = time.dwLowDateTime;
        ticks.HighPart = time.dwHighDateTime;

        wchar_t buffer[40]{};
        swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02u.%07uZ",
            utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, static_cast<unsigned>(ticks.QuadPart % 10000000ull));

        return buffer;
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
        definition.ServiceInstanceName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
        definition.AllowPortFallback = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY, true);
        definition.Advertise = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY, true);
        definition.Enabled = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);
        definition.SendRecoveryJournal = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);
        definition.RemoteClientPolicy = ReadRemoteClientPolicy(entry);

        if (!TryReadName(entry, definition.Name, errorCode, messageId)) return false;
        if (!IsValidName(definition.Name, false, errorCode, messageId)) return false;

        // checked even when the host is not advertised, because advertising it later uses this name
        if (!definition.ServiceInstanceName.empty() && !IsValidName(definition.ServiceInstanceName, true, errorCode, messageId)) return false;
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
        definition.RemoteServiceInstanceName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
        definition.RemoteAddress = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
        definition.CustomEndpointName = RtpMidiJson::GetString(entry, MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY);
        definition.AutoReconnect = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY, true);
        definition.Enabled = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY, true);
        definition.SendRecoveryJournal = RtpMidiJson::GetBoolean(entry, MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY, true);

        if (!TryReadName(entry, definition.Name, errorCode, messageId)) return false;
        if (!IsValidName(definition.Name, false, errorCode, messageId)) return false;

        // only this PC uses these, but they come from a file anyone can edit
        if (definition.CustomEndpointName.size() > MIDI_RTP_CONFIG_TEXT_MAX_CHARS ||
            definition.RemoteAddress.size() > MIDI_RTP_CONFIG_TEXT_MAX_CHARS)
        {
            errorCode = RTP_MIDI_ERROR_CODE_NAME_TOO_LONG;
            messageId = IDS_RTP_ERROR_TEXT_TOO_LONG;
            return false;
        }

        // longer than a DNS-SD label, so it could never match anything on the network
        if (RtpMidiText::Utf8ByteCount(definition.RemoteServiceInstanceName) > MIDI_RTP_NAME_MAX_UTF8_BYTES)
        {
            errorCode = RTP_MIDI_ERROR_CODE_NAME_TOO_LONG;
            messageId = IDS_RTP_ERROR_NAME_TOO_LONG;
            return false;
        }

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

    // Not trimmed: it has to match what the remote sends, byte for byte apart from case
    bool TryGetRemoteNameArgument(
        _In_ internal::MidiTransportCommandHelper& command,
        _Inout_ json::JsonObject& responseObject,
        _Out_ std::wstring& remoteName)
    {
        remoteName.clear();

        auto const arguments = command.Arguments();

        if (arguments == nullptr)
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_MISSING_REMOTE_CLIENT_NAME, IDS_RTP_ERROR_MISSING_REMOTE_CLIENT_NAME);
            return false;
        }

        auto const found = arguments->find(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_REMOTE_NAME);

        if (found == arguments->end() || found->second.empty())
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_MISSING_REMOTE_CLIENT_NAME, IDS_RTP_ERROR_MISSING_REMOTE_CLIENT_NAME);
            return false;
        }

        if (found->second.size() > MIDI_RTP_REMOTE_CLIENT_NAME_MAX_CHARS)
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_NAME_TOO_LONG, IDS_RTP_ERROR_TEXT_TOO_LONG);
            return false;
        }

        remoteName = found->second;
        return true;
    }

    bool TryGetScopeArgument(
        _In_ internal::MidiTransportCommandHelper& command,
        _Inout_ json::JsonObject& responseObject,
        _Out_ RtpMidiApprovalScope& scope)
    {
        scope = RtpMidiApprovalScope::Once;

        auto const arguments = command.Arguments();

        if (arguments != nullptr)
        {
            auto const found = arguments->find(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE);

            if (found != arguments->end())
            {
                auto const value = internal::TrimmedWStringCopy(found->second);

                if (_wcsicmp(value.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ONCE) == 0) { scope = RtpMidiApprovalScope::Once; return true; }
                if (_wcsicmp(value.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_UNTIL_RESTART) == 0) { scope = RtpMidiApprovalScope::UntilRestart; return true; }
                if (_wcsicmp(value.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ALWAYS) == 0) { scope = RtpMidiApprovalScope::Always; return true; }
            }
        }

        Fail(responseObject, RTP_MIDI_ERROR_CODE_INVALID_APPROVAL_SCOPE, IDS_RTP_ERROR_INVALID_APPROVAL_SCOPE);
        return false;
    }

    UINT MessageForDecisionError(_In_ uint32_t const errorCode) noexcept
    {
        switch (errorCode)
        {
        case RTP_MIDI_ERROR_CODE_PENDING_REMOTE_CLIENT_NOT_FOUND: return IDS_RTP_ERROR_PENDING_REMOTE_CLIENT_NOT_FOUND;
        case RTP_MIDI_ERROR_CODE_TOO_MANY_REMOTE_CLIENT_DECISIONS: return IDS_RTP_ERROR_TOO_MANY_REMOTE_CLIENT_DECISIONS;
        default: return IDS_RTP_ERROR_ENTRY_NOT_FOUND;
        }
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

    // after the hosts, so decisions saved beside a host in the file find it defined
    json::JsonObject decisions{ nullptr };
    if (RtpMidiJson::TryGetObject(createSection, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_DECISIONS_KEY, decisions))
    {
        for (auto const& pair : decisions)
        {
            auto const key = pair.Key();

            GUID hostId{};
            if (!RtpMidiJson::TryParseEntryIdentifier(internal::TrimmedWStringCopy(std::wstring{ key }), hostId))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY_IDENTIFIER, IDS_RTP_ERROR_INVALID_ENTRY_IDENTIFIER);
                continue;
            }

            json::JsonObject entry{ nullptr };
            if (!RtpMidiJson::TryGetObject(decisions, key, entry))
            {
                reportFailure(key, RTP_MIDI_ERROR_CODE_INVALID_ENTRY, IDS_RTP_ERROR_INVALID_ENTRY);
                continue;
            }

            // left over from a host which is gone
            RtpMidiHostDefinition host{};
            if (!TransportState::Current().TryGetHostDefinition(hostId, host)) continue;

            TransportState::Current().Approvals().SetRememberedDecisions(
                hostId,
                ReadRemoteClientNames(entry, MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY),
                ReadRemoteClientNames(entry, MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY));
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
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS, true);

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
        responseObject.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_HOSTS_KEY, endpointManager->BuildAdvertisedHostsJson());
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

        RtpMidiHostDefinition host{};
        RtpMidiClientDefinition client{};

        if (!TransportState::Current().TryGetHostDefinition(entryId, host) && !TransportState::Current().TryGetClientDefinition(entryId, client))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        if (FAILED(endpointManager->DisconnectConnection(entryId, connectionId)))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_CONNECTION_NOT_FOUND, IDS_RTP_ERROR_CONNECTION_NOT_FOUND);
            return;
        }

        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT || verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT)
    {
        GUID hostId{};
        std::wstring remoteName;
        RtpMidiApprovalScope scope{ RtpMidiApprovalScope::Once };

        if (!TryGetEntryArgument(command, responseObject, hostId)) return;
        if (!TryGetRemoteNameArgument(command, responseObject, remoteName)) return;
        if (!TryGetScopeArgument(command, responseObject, scope)) return;

        bool const allow = verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT;

        uint32_t errorCode{ 0 };
        if (FAILED(TransportState::Current().Approvals().Decide(hostId, remoteName, allow, scope, errorCode)))
        {
            Fail(responseObject, errorCode, MessageForDecisionError(errorCode));
            return;
        }

        // a refused remote which is already connected is let go as well
        if (!allow) endpointManager->EndConnectionsFromRemote(hostId, remoteName);

        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT)
    {
        GUID hostId{};
        std::wstring remoteName;

        if (!TryGetEntryArgument(command, responseObject, hostId)) return;
        if (!TryGetRemoteNameArgument(command, responseObject, remoteName)) return;

        if (FAILED(TransportState::Current().Approvals().Forget(hostId, remoteName)))
        {
            Fail(responseObject, RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND, IDS_RTP_ERROR_ENTRY_NOT_FOUND);
            return;
        }

        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }
    else if (verb == MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS)
    {
        json::JsonArray pendingArray;

        for (auto const& pending : TransportState::Current().Approvals().PendingRemoteClients())
        {
            RtpMidiHostDefinition host{};
            TransportState::Current().TryGetHostDefinition(pending.HostId, host);

            json::JsonObject item;
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY, json::JsonValue::CreateStringValue(internal::GuidToString(pending.HostId)));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY, json::JsonValue::CreateStringValue(pending.RemoteName));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY, json::JsonValue::CreateStringValue(pending.RemoteAddress));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PENDING_HOST_NAME_KEY, json::JsonValue::CreateStringValue(host.Name));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PENDING_HOST_SERVICE_INSTANCE_NAME_KEY, json::JsonValue::CreateStringValue(host.EffectiveServiceInstanceName()));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REQUEST_TIME_KEY, json::JsonValue::CreateStringValue(ToIso8601(pending.FirstRequest)));
            item.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PENDING_APPROVED_KEY, json::JsonValue::CreateBooleanValue(pending.Approved));

            pendingArray.Append(item);
        }

        responseObject.SetNamedValue(MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REMOTE_CLIENTS_KEY, pendingArray);
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
