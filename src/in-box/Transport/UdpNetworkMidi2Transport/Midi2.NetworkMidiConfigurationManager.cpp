// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "Feature_Servicing_MIDI2EndpointCustomizationEnhancements.h"

using namespace winrt::Windows::Networking;

namespace
{
    // JsonObject::GetNamedObject throws when the name is present but holds something other
    // than an object, and every value here comes from a caller who can put any type anywhere.
    // The two-argument overload only covers the name being absent.
    void TraceWrongJsonType(_In_ winrt::hstring const& name, _In_ wchar_t const* const expected) noexcept;

    json::JsonObject SafeGetNamedObject(_In_ json::JsonObject const& parent, _In_ winrt::hstring const& name) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(name))
            {
                return nullptr;
            }

            auto value = parent.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
            {
                TraceWrongJsonType(name, L"object");
                return nullptr;
            }

            // safe now that the type has been checked
            return parent.GetNamedObject(name);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // A key that is present but holds the wrong type is a mistake in the configuration, not a
    // normal absence, and the fallback is otherwise invisible: the customer sees a host that is
    // silently not enabled, with nothing in the log to explain it.
    void TraceWrongJsonType(_In_ winrt::hstring const& name, _In_ wchar_t const* const expected) noexcept
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingWideString(L"Configuration value is the wrong type. The default was used instead.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(name.c_str(), "key"),
            TraceLoggingWideString(expected, "expected type")
        );
    }

    // Same hazard as SafeGetNamedObject, for the other value types. The two-argument
    // GetNamedString / GetNamedBoolean / GetNamedNumber overloads only cover the name being
    // ABSENT: a name that is present holding the wrong type still throws, and this runs on a
    // COM boundary inside the service.
    winrt::hstring SafeGetNamedString(
        _In_ json::JsonObject const& parent,
        _In_ winrt::hstring const& name,
        _In_ winrt::hstring const& defaultValue) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(name))
            {
                return defaultValue;
            }

            auto value = parent.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                TraceWrongJsonType(name, L"string");
                return defaultValue;
            }

            return value.GetString();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    bool SafeGetNamedBoolean(
        _In_ json::JsonObject const& parent,
        _In_ winrt::hstring const& name,
        _In_ bool const defaultValue) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(name))
            {
                return defaultValue;
            }

            auto value = parent.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Boolean)
            {
                TraceWrongJsonType(name, L"boolean");
                return defaultValue;
            }

            return value.GetBoolean();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    // A number outside the permitted range is treated as absent. Config files are hand-edited,
    // and clamping silently would hide the mistake from whoever wrote it.
    uint8_t SafeGetNamedByte(
        _In_ json::JsonObject const& parent,
        _In_ winrt::hstring const& name,
        _In_ uint8_t const defaultValue,
        _In_ uint8_t const minimumValue,
        _In_ uint8_t const maximumValue) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(name))
            {
                return defaultValue;
            }

            auto value = parent.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                TraceWrongJsonType(name, L"number");
                return defaultValue;
            }

            auto const number = value.GetNumber();

            if (number < minimumValue || number > maximumValue)
            {
                return defaultValue;
            }

            return static_cast<uint8_t>(number);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    json::JsonArray SafeGetNamedArray(_In_ json::JsonObject const& parent, _In_ winrt::hstring const& name) noexcept
    {
        try
        {
            if (parent == nullptr || !parent.HasKey(name))
            {
                return nullptr;
            }

            auto value = parent.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                TraceWrongJsonType(name, L"array");
                return nullptr;
            }

            return value.GetArray();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // Transport settings are corrected rather than refused, so anything the caller writes has to
    // resolve to a usable number: absent, the wrong type, fractional, negative, or beyond the
    // supported range all land on something the transport can actually run with.
    uint32_t ReadClampedTransportSetting(
        _In_ json::JsonObject const& section,
        _In_ winrt::hstring const& name,
        _In_ uint32_t const defaultValue,
        _In_ uint32_t const lowerBound,
        _In_ uint32_t const upperBound,
        _Inout_ bool& anyAdjusted) noexcept
    {
        double rawValue{ 0.0 };

        try
        {
            if (section == nullptr || !section.HasKey(name))
            {
                return defaultValue;
            }

            auto value = section.Lookup(name);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                anyAdjusted = true;
                return defaultValue;
            }

            rawValue = value.GetNumber();
        }
        catch (...)
        {
            anyAdjusted = true;
            return defaultValue;
        }

        // NaN compares false against everything, so it would otherwise slip past both bounds
        if (std::isnan(rawValue))
        {
            anyAdjusted = true;
            return defaultValue;
        }

        if (rawValue < static_cast<double>(lowerBound))
        {
            anyAdjusted = true;
            return lowerBound;
        }

        if (rawValue > static_cast<double>(upperBound))
        {
            anyAdjusted = true;
            return upperBound;
        }

        return static_cast<uint32_t>(rawValue);
    }

    // Entry identifiers are GUIDs. winrt::guid's string constructor validates length, separators
    // and every hex digit, and takes both the braced and unbraced forms, but it throws
    // std::invalid_argument rather than returning a failure.
    bool TryParseEntryIdentifier(_In_ winrt::hstring const& value, _Out_ winrt::guid& result) noexcept
    {        result = winrt::guid{};

        if (value.empty())
        {
            return false;
        }

        try
        {
            result = winrt::guid{ std::wstring_view{ value } };

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager
)
{
    UNREFERENCED_PARAMETER(transportId);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManager);


    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
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



MidiNetworkHostAuthentication 
MidiNetworkHostAuthenticationFromJsonString(_In_ winrt::hstring const& jsonString)
{
    auto value = internal::ToLowerTrimmedHStringCopy(jsonString);

    if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE)
    {
        return MidiNetworkHostAuthentication::NoAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD)
    {
        return MidiNetworkHostAuthentication::PasswordAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER)
    {
        return MidiNetworkHostAuthentication::UserAuthentication;
    }
    else
    {
        // default is any
        return MidiNetworkHostAuthentication::NoAuthentication;
    }
}


MidiNetworkRemoteClientPolicy
MidiNetworkRemoteClientPolicyFromJsonString(_In_ winrt::hstring const& jsonString)
{
    auto value = internal::ToLowerTrimmedHStringCopy(jsonString);

    if (value == internal::ToLowerTrimmedHStringCopy(winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL }))
    {
        return MidiNetworkRemoteClientPolicy::PolicyRequireApproval;
    }

    // default is to allow any, which is what a host with no configured policy has always done
    return MidiNetworkRemoteClientPolicy::PolicyAllowAny;
}

winrt::hstring
EntryStateToString(_In_ MidiNetworkEntryState const state)
{
    switch (state)
    {
    case MidiNetworkEntryState::Live:
        return winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_LIVE };

    case MidiNetworkEntryState::Failed:
        return winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_FAILED };

    case MidiNetworkEntryState::Unavailable:
        return winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE };

    default:
        return winrt::hstring{ MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_PENDING };
    }
}

// Reads an array of { umpEndpointName, productInstanceId } objects into comparison keys.
// Entries missing either field cannot identify a device, so they are skipped rather than
// stored as something which would match nothing or, worse, everything.
void
ReadRemoteClientIdentityList(
    _In_ json::JsonObject const& parent,
    _In_ std::wstring const& arrayKey,
    _Inout_ std::vector<std::wstring>& keys)
{
    auto entries = SafeGetNamedArray(parent, winrt::hstring{ arrayKey });

    if (entries == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < entries.Size(); i++)
    {
        auto element = entries.GetAt(i);

        // an array can hold anything, so a non-object element is skipped rather than fetched
        // as one, which would throw. windows.h renames IJsonValue::GetObject, so the fetch has
        // to go through JsonArray::GetObjectAt, which is unaffected.
        if (element == nullptr || element.ValueType() != json::JsonValueType::Object)
        {
            continue;
        }

        auto entry = entries.GetObjectAt(i);

        if (entry == nullptr)
        {
            continue;
        }

        MidiNetworkRemoteClientIdentity identity{};

        identity.UmpEndpointName = internal::TrimmedWStringCopy(std::wstring{ SafeGetNamedString(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L"") });
        identity.ProductInstanceId = internal::TrimmedWStringCopy(std::wstring{ SafeGetNamedString(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L"") });

        if (!identity.IsValid())
        {
            continue;
        }

        auto key = identity.Key();

        if (std::find(keys.begin(), keys.end(), key) == keys.end())
        {
            keys.push_back(key);
        }
    }
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ValidateHostDefinition(
    MidiNetworkHostDefinition& definition,
    winrt::hstring& errorMessage,
    uint32_t& errorCode)
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        errorMessage = L"";
        errorCode = NETWORK_ERROR_CODE_UNKNOWN_ERROR;

        // is there a unique identifier?

        if (definition.EntryIdentifier == winrt::guid{})
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER);
            errorCode = NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER;
            return E_INVALIDARG;
        }

        // TODO: was that identifier already in use?


        if (definition.UmpEndpointName.empty())
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_NAME);
            errorCode = NETWORK_ERROR_CODE_MISSING_ENDPOINT_NAME;
            return E_INVALIDARG;
        }

        // Enforced here as well as in MidiNetworkHost::Initialize, because a definition rejected
        // there is skipped silently, which looked to the caller like a host that was created and
        // then vanished. The limit is a UTF-8 byte count, so hstring::size() is the wrong measure.
        if (internal::ExceedsUtf8ByteCount(std::wstring{ definition.UmpEndpointName }, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT))
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_ENDPOINT_NAME_TOO_LONG);
            errorCode = NETWORK_ERROR_CODE_ENDPOINT_NAME_TOO_LONG;
            return E_INVALIDARG;
        }

        if (definition.ProductInstanceId.empty())
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_PRODUCT_INSTANCE_ID);
            errorCode = NETWORK_ERROR_CODE_MISSING_PRODUCT_INSTANCE_ID;
            return E_INVALIDARG;
        }

        if (internal::ExceedsUtf8ByteCount(std::wstring{ definition.ProductInstanceId }, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT))
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_PRODUCT_INSTANCE_ID_TOO_LONG);
            errorCode = NETWORK_ERROR_CODE_PRODUCT_INSTANCE_ID_TOO_LONG;
            return E_INVALIDARG;
        }

        // Spec range is ASCII 32-126, which is wider than a device identifier allows. The value is
        // kept as supplied because it is meaningful on the device; it gets stripped only where it is
        // used to build an SWD id.
        if (!internal::ContainsOnlyPrintableAscii(std::wstring{ definition.ProductInstanceId }))
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_PRODUCT_INSTANCE_ID);
            errorCode = NETWORK_ERROR_CODE_INVALID_PRODUCT_INSTANCE_ID;
            return E_INVALIDARG;
        }

        // A manually assigned port has to be a real port number. An unparseable or out-of-range
        // value used to be carried all the way to the socket bind, which failed much later and much
        // less clearly.
        if (!definition.UseAutomaticPortAllocation)
        {
            auto const portText = internal::TrimmedWStringCopy(std::wstring{ definition.Port });

            bool valid = !portText.empty() &&
                std::all_of(portText.begin(), portText.end(), [](wchar_t const ch) { return iswdigit(ch) != 0; });

            if (valid)
            {
                auto const value = wcstoul(portText.c_str(), nullptr, 10);

                valid = (value >= 1 && value <= 65535);
            }

            if (!valid)
            {
                errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_HOST_PORT);
                errorCode = NETWORK_ERROR_CODE_INVALID_HOST_PORT;
                return E_INVALIDARG;
            }
        }

        // Refused rather than silently downgraded. Accepting unauthenticated invitations on a
        // host the user asked to protect would be worse than refusing to start it. What adding
        // authentication would take is in MidiNetworkCredentials.h.
        // https://github.com/microsoft/MIDI/issues/733
        if (definition.Authentication != MidiNetworkHostAuthentication::NoAuthentication)
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_AUTHENTICATION_NOT_IMPLEMENTED);
            errorCode = NETWORK_ERROR_CODE_AUTHENTICATION_NOT_IMPLEMENTED;
            return E_NOTIMPL;
        }

        // The SDK truncates this on the way in, but a configuration file written by hand reaches
        // here without ever passing through it, and an over-long label goes straight to
        // DnsServiceRegister.
        if (internal::ExceedsUtf8ByteCount(std::wstring{ definition.ServiceInstanceName }, MIDI_DNSSD_SERVICE_INSTANCE_NAME_MAX_BYTE_COUNT))
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_SERVICE_INSTANCE_NAME_TOO_LONG);
            errorCode = NETWORK_ERROR_CODE_SERVICE_INSTANCE_NAME_TOO_LONG;
            return E_INVALIDARG;
        }

        return S_OK;

    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandStopHost(
    winrt::guid const& hostEntryId,
    json::JsonObject& responseObject) noexcept
try
{
    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    if (SUCCEEDED(host->Stop()))
    {
        internal::SetConfigurationResponseObjectSuccess(responseObject);
        return S_OK;
    }
    else
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_UNABLE_TO_STOP_HOST, internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_STOP_HOST));
        return S_OK;
    }
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}

// Stops the host and removes the entry. stopHost deliberately keeps the entry so the host can be
// started again, which also means it keeps holding its service instance name. This is the verb
// which gives that name back, and the only way to be rid of a host without restarting the
// service.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandRemoveHost(
    winrt::guid const& hostEntryId,
    json::JsonObject& responseObject) noexcept
try
{
    // Detached first, so nothing can start it again while it is being shut down, and so the
    // state lock is not held across a Shutdown which blocks on the network.
    bool removedPendingDefinition{ false };

    auto host = TransportState::Current().RemoveHost(hostEntryId, removedPendingDefinition);

    if (host == nullptr)
    {
        // A host which was accepted but not yet built exists only as a pending definition.
        // Taking that away is still a successful removal as far as the caller is concerned.
        if (removedPendingDefinition)
        {
            internal::SetConfigurationResponseObjectSuccess(responseObject);
            return S_OK;
        }

        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    // Sends the Byes, tears down the connections, unbinds the port and removes the parent device.
    LOG_IF_FAILED(host->Shutdown());

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandStartHost(
    winrt::guid const& hostEntryId,
    json::JsonObject& responseObject) noexcept
try
{
    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    auto startHr = host->Start();

    if (SUCCEEDED(startHr))
    {
        internal::SetConfigurationResponseObjectSuccess(responseObject);
        return S_OK;
    }
    else if (host->IsWaitingForNetworkAdapter())
    {
        // Not a failure the caller can fix by trying again. It starts when the adapter is back.
        internal::SetConfigurationResponseObjectFailWithErrorCode(
            responseObject,
            NETWORK_ERROR_CODE_NETWORK_ADAPTER_NOT_AVAILABLE,
            internal::ResourceGetWString(IDS_ERROR_NETWORK_ADAPTER_NOT_AVAILABLE));

        return S_OK;
    }
    else
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(
            responseObject,
            NETWORK_ERROR_CODE_UNABLE_TO_START_HOST,
            internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_START_HOST));

        return S_OK;
    }
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}



_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandConnectDirect(
    winrt::guid const& configEntryId,
    winrt::hstring const& remoteAddress,
    winrt::hstring const& remotePort,
    winrt::hstring const& umpEndpointName,
    winrt::hstring const& customEndpointName,
    bool const createMidi1Ports,
    uint8_t const fallbackMidi1PortCount,
    json::JsonObject& responseObject) noexcept
try
{

    if (remoteAddress.empty())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_REMOTE_ADDRESS, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_ADDRESS));
        return S_OK;
    }

    if (remotePort.empty())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_REMOTE_PORT, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_PORT));
        return S_OK;
    }

    // do some validation of the remote port
    auto num = wcstol(remotePort.c_str(), NULL, 10);
    if (num > WORD_MAX || num < 1)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_INVALID_REMOTE_PORT, internal::ResourceGetWString(IDS_ERROR_INVALID_REMOTE_PORT));
        return S_OK;
    }

    // this happens all in real-time, unlike the stuff that is done via the config file

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    // The same entry arriving again is the app saying the remote is available now. A direct IP
    // address which gave up earlier is only ever revived here.
    if (TransportState::Current().RearmClientDefinition(configEntryId) == S_OK)
    {
        LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());

        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    MidiNetworkClientDefinition clientDefinition{ };

    clientDefinition.CreateMidi1Ports = createMidi1Ports;
    clientDefinition.FallbackMidi1PortCount = fallbackMidi1PortCount;
    clientDefinition.EntryIdentifier = configEntryId;
    clientDefinition.MatchDirectHostNameOrIPAddress = remoteAddress;
    clientDefinition.MatchDirectPort = remotePort;
    clientDefinition.LocalEndpointName = umpEndpointName;
    clientDefinition.CustomEndpointName = customEndpointName;

    // Registered before it is started, or nothing can retry it, revive it, or report it in
    // enumerateClients.
    LOG_IF_FAILED(TransportState::Current().AddClientDefinition(clientDefinition));

    // Started by the creator worker, not here. Starting it here too raced the worker into
    // building the same client twice, and held this call for up to the connect timeout.
    LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandConnectMdns(
    winrt::guid const& configEntryId,
    winrt::hstring const& matchId,
    winrt::hstring const& umpEndpointName,
    winrt::hstring const& customEndpointName,
    bool const createMidi1Ports,
    uint8_t const fallbackMidi1PortCount,
    json::JsonObject& responseObject) noexcept
try
{
    if (matchId.empty())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_MATCH_ID, internal::ResourceGetWString(IDS_ERROR_MISSING_MATCH_ID));
        return S_OK;
    }

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    // The same entry arriving again is the app saying "try this one now", exactly as it is for
    // a direct connection.
    if (TransportState::Current().RearmClientDefinition(configEntryId) == S_OK)
    {
        LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());

        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    MidiNetworkClientDefinition clientDefinition{ };

    clientDefinition.CreateMidi1Ports = createMidi1Ports;
    clientDefinition.FallbackMidi1PortCount = fallbackMidi1PortCount;
    clientDefinition.EntryIdentifier = configEntryId;
    clientDefinition.MatchId = matchId;
    clientDefinition.LocalEndpointName = umpEndpointName;
    clientDefinition.CustomEndpointName = customEndpointName;

    LOG_IF_FAILED(TransportState::Current().AddClientDefinition(clientDefinition));

    // Unlike a direct connection there is nothing to start here. The endpoint creator thread
    // owns the match: it connects as soon as the advertised host is present, and again whenever
    // it comes back, without the caller having to know an address.
    LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandDisconnectClient(
    winrt::guid const& configEntryId,
    json::JsonObject& responseObject) noexcept
try
{

    if (configEntryId == winrt::guid{})
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }


    auto client = TransportState::Current().GetClient(configEntryId);

    // A client which never connected exists only as a definition. Disconnecting it is still a
    // real removal, so it must not be reported as "not found" - the entry is what the user sees
    // in enumerateClients, and leaving it behind is what made a disconnected row linger.
    if (client != nullptr)
    {
        LOG_IF_FAILED(client->DisconnectByUser());
    }

    bool removedPendingDefinition{ false };

    if (FAILED(TransportState::Current().RemoveClient(configEntryId, removedPendingDefinition)) && client == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_CLIENT_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_CLIENT_NOT_FOUND));
        return S_OK;
    }

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


// A user decision about a remote client, arriving from the settings app after it polled and saw
// something in the pending state. The identity, not the address, is what is recorded: a client
// reconnects from a new ephemeral port every time.
//
// "always" and "denyAlways" also need writing to the configuration file by the caller so they
// survive a restart. The service applies every scope immediately, and holds "untilRestart"
// denials only in memory, which is exactly what that scope means.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandRemoteClientDecision(
    winrt::guid const& hostEntryId,
    MidiNetworkRemoteClientIdentity const& identity,
    bool const approve,
    bool const persist,
    json::JsonObject& responseObject) noexcept
try
{
    if (hostEntryId == winrt::guid{})
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }

    if (!identity.IsValid())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_REMOTE_CLIENT_IDENTITY, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));
        return S_OK;
    }

    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    // "once" is deliberately not recorded anywhere. It authorizes the connection which is
    // waiting right now and nothing beyond it.
    if (persist)
    {
        if (approve)
        {
            LOG_IF_FAILED(host->AddRemoteClientToAllowList(identity));
        }
        else
        {
            LOG_IF_FAILED(host->AddRemoteClientToDenyList(identity));
        }
    }
    else if (!approve)
    {
        // "untilRestart". Held in memory only, so a service restart clears it.
        LOG_IF_FAILED(host->AddRemoteClientToDenyList(identity));
    }

    // Release, or refuse, whatever this identity holds. There can be more than one connection if
    // the client retried from a new port while the user was deciding.
    auto key = identity.Key();

    for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(hostEntryId))
    {
        if (connection == nullptr)
        {
            continue;
        }

        if (connection->GetRemoteClientIdentity().Key() != key)
        {
            continue;
        }

        if (connection->IsAwaitingUserApproval())
        {
            if (approve)
            {
                LOG_IF_FAILED(connection->ApproveByUser());
            }
            else
            {
                LOG_IF_FAILED(connection->DenyByUser());
            }
        }
        else if (!approve)
        {
            // A deny has to reach an established session as well. The lists are only consulted
            // when an invitation arrives, and a live session is never re-invited, so leaving it
            // alone would keep a blocked remote streaming until it happened to reconnect.
            LOG_IF_FAILED(connection->DisconnectByUser());
        }
    }

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


// Ends the sessions a single remote client holds with one of our hosts. Deliberately records
// nothing: the user asked for this connection to stop, not for the remote to be refused in
// future. denyRemoteClient with an "always" scope is what does that.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandDisconnectRemoteClient(
    winrt::guid const& hostEntryId,
    MidiNetworkRemoteClientIdentity const& identity,
    json::JsonObject& responseObject) noexcept
try
{
    if (hostEntryId == winrt::guid{})
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }

    if (!identity.IsValid())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_REMOTE_CLIENT_IDENTITY, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));
        return S_OK;
    }

    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    auto key = identity.Key();

    // One identity can hold more than one connection here: it may have re-invited from a new
    // ephemeral port while an earlier connection was still being reaped.
    bool disconnectedAny{ false };

    for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(hostEntryId))
    {
        if (connection == nullptr)
        {
            continue;
        }

        if (connection->GetRemoteClientIdentity().Key() != key)
        {
            continue;
        }

        LOG_IF_FAILED(connection->DisconnectByUser());

        disconnectedAny = true;
    }

    // Saying a disconnect happened when no such connection existed would leave a caller
    // believing it had acted on something.
    if (!disconnectedAny)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_REMOTE_CLIENT_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_REMOTE_CLIENT_NOT_FOUND));
        return S_OK;
    }

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


// Drops a remembered allow or deny so the running service stops applying it. The caller rewrites
// the configuration file as well; without this the old decision would stand until the service
// restarted. Any live session is left alone, because forgetting is not blocking.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandForgetRemoteClient(
    winrt::guid const& hostEntryId,
    MidiNetworkRemoteClientIdentity const& identity,
    json::JsonObject& responseObject) noexcept
try
{
    if (hostEntryId == winrt::guid{})
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_ENTRY_IDENTIFIER, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }

    if (!identity.IsValid())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_REMOTE_CLIENT_IDENTITY, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));
        return S_OK;
    }

    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_HOST_NOT_FOUND, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    // An identity the host holds no decision for is not an error. What the caller asked for,
    // that nothing is remembered about it, is already true.
    LOG_IF_FAILED(host->ForgetRemoteClient(identity));

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


namespace
{
    // Calls visit with each well-formed element of an endpoint customization array and the
    // match it carries. One malformed element costs only itself.
    template <typename TVisit>
    void ForEachEndpointCustomizationEntry(
        _In_ json::JsonArray const& entries,
        _In_ TVisit&& visit)
    {
        // Indexed rather than ranged, because windows.h renames IJsonValue::GetObject and
        // JsonArray::GetObjectAt is unaffected.
        for (uint32_t i = 0; i < entries.Size(); i++)
        {
            auto element = entries.GetAt(i);

            if (element == nullptr || element.ValueType() != json::JsonValueType::Object)
            {
                continue;
            }

            auto entryObject = entries.GetObjectAt(i);

            if (entryObject == nullptr)
            {
                continue;
            }

            auto matchObject = SafeGetNamedObject(
                entryObject, WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey);

            if (matchObject == nullptr)
            {
                // nothing to tie this entry to
                continue;
            }

            auto matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);

            if (matchCriteria == nullptr)
            {
                continue;
            }

            visit(entryObject, matchCriteria);
        }
    }
}

_Use_decl_annotations_
void
CMidi2NetworkMidiConfigurationManager::CacheAndApplyEndpointCustomization(
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> const& matchCriteria,
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> const& customProperties)
{
    // Cached whether or not the endpoint exists yet. A network endpoint is created only
    // when the remote answers, which is normally after this arrives, and the creation
    // path reads this cache before it activates the device node.
    LOG_HR_IF(E_FAIL, !m_customPropertiesCache->Add(matchCriteria, customProperties));

    // An endpoint which is already live is updated in place, so a rename after the fact
    // still works without recreating the connection.
    auto endpointManager = TransportState::Current().GetEndpointManager();

    if (endpointManager == nullptr)
    {
        return;
    }

    auto existingEndpointDeviceId = endpointManager->FindMatchingInstantiatedEndpoint(*matchCriteria);

    if (existingEndpointDeviceId.empty())
    {
        return;
    }

    std::vector<DEVPROPERTY> endpointDevProperties{};

    if (customProperties->WriteAllProperties(endpointDevProperties) && endpointDevProperties.size() > 0)
    {
        LOG_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
            existingEndpointDeviceId.c_str(),
            static_cast<ULONG>(endpointDevProperties.size()),
            endpointDevProperties.data()));

        // The name above is the endpoint's. The MIDI 1.0 ports take their names from a
        // separate table, which nothing here has rewritten, so without this a renamed
        // device keeps its old port names. The name has to be handed over rather than
        // read back, because the endpoint does not report it yet. An empty name puts back
        // the one the remote supplied. Zero keeps the port count as it is, because a rename
        // is not a reason to change it.
        LOG_IF_FAILED(endpointManager->RefreshMidi1PortsForEndpoint(
            std::wstring{ existingEndpointDeviceId },
            0,
            std::wstring{ customProperties->Name }));
    }
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ProcessEndpointCustomizations(
    json::JsonObject const& jsonObject,
    json::JsonObject& responseObject) noexcept
{
    try
    {
        auto updateArray = SafeGetNamedArray(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY);

        if (updateArray == nullptr || updateArray.Size() == 0)
        {
            return S_OK;
        }

        // The caller assumes failure until something says otherwise, and a payload which carries
        // only customizations reaches none of the places which say so.
        bool anyCustomizationAccepted{ false };

        ForEachEndpointCustomizationEntry(
            updateArray,
            [&](json::JsonObject const& updateObject, auto const& matchCriteria)
            {
                auto customPropertiesObject = SafeGetNamedObject(
                    updateObject, WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey);

                if (customPropertiesObject == nullptr)
                {
                    return;
                }

                std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties{ nullptr };

                if (Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
                {
                    customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJsonRejectingImagePath(
                        customPropertiesObject);
                }
                else
                {
                    customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJson(
                        customPropertiesObject);
                }

                if (customProperties == nullptr)
                {
                    return;
                }

                CacheAndApplyEndpointCustomization(matchCriteria, customProperties);

                anyCustomizationAccepted = true;

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Cached endpoint customization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(customProperties->Name.c_str(), "custom name"),
                    TraceLoggingWideString(matchCriteria->DeviceProductInstanceId.c_str(), "product instance id")
                );
            });

        if (anyCustomizationAccepted)
        {
            internal::SetConfigurationResponseObjectSuccess(responseObject);
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
CMidi2NetworkMidiConfigurationManager::ProcessEndpointCustomizationRemovals(
    json::JsonObject const& removeSection,
    json::JsonObject& responseObject) noexcept
{
    try
    {
        // Same array shape as a customization, carrying only the match
        auto removeArray = SafeGetNamedArray(removeSection, MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY);

        if (removeArray == nullptr || removeArray.Size() == 0)
        {
            return S_OK;
        }

        bool anyRemovalAccepted{ false };

        ForEachEndpointCustomizationEntry(
            removeArray,
            [&](json::JsonObject const&, auto const& matchCriteria)
            {
                // Every member defaults to the uncustomized value, and writing them sends
                // DEVPROP_TYPE_EMPTY for each string, which deletes it. The cache replaces the
                // entry that matches, so this both withdraws the cached customization and undoes
                // the applied one.
                CacheAndApplyEndpointCustomization(
                    matchCriteria,
                    std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties>());

                anyRemovalAccepted = true;

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Removed endpoint customization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(matchCriteria->TransportSuppliedEndpointName.c_str(), "transport supplied name"),
                    TraceLoggingWideString(matchCriteria->DeviceProductInstanceId.c_str(), "product instance id")
                );
            });

        if (anyRemovalAccepted)
        {
            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
    }
    catch (...)
    {
        RETURN_IF_FAILED(E_FAIL);
    }

    return S_OK;
}


namespace
{
    // One entry of the enumerateClients response: what was configured, and what the running
    // client reports when there is one.
    json::JsonObject BuildEnumeratedClientObject(
        _In_ MidiNetworkClientDefinition const& definition,
        _In_ std::shared_ptr<MidiNetworkClient> const& client)
    {
        json::JsonObject clientObject;

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY,
            json::JsonValue::CreateStringValue(winrt::hstring{ internal::GuidToString(definition.EntryIdentifier) }));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_MDNS_MATCH_ID_KEY,
            json::JsonValue::CreateStringValue(definition.MatchId));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_DIRECT_KEY,
            json::JsonValue::CreateBooleanValue(definition.IsDirectConnection()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(definition.MatchDirectHostNameOrIPAddress));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_PORT_KEY,
            json::JsonValue::CreateStringValue(definition.MatchDirectPort));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_ENTRY_STATE_KEY,
            json::JsonValue::CreateStringValue(EntryStateToString(definition.State)));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(definition.CreateMidi1Ports));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_FALLBACK_MIDI1_PORT_COUNT_KEY,
            json::JsonValue::CreateNumberValue(definition.FallbackMidi1PortCount));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_SESSION_ACTIVE_KEY,
            json::JsonValue::CreateBooleanValue(client != nullptr && client->IsSessionActive()));

        if (client == nullptr)
        {
            return clientObject;
        }

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(client->RemoteAddress()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_PORT_KEY,
            json::JsonValue::CreateStringValue(client->RemotePort()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(client->LocalAddress()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_PORT_KEY,
            json::JsonValue::CreateStringValue(client->LocalPort()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_UMP_ENDPOINT_ID_KEY,
            json::JsonValue::CreateStringValue(client->GetEndpointDeviceId()));

        // TODO: possibly move this to a different command to make the payload smaller
        auto latency = client->GetAndResetAverageLatencyTicks();

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CURRENT_LATENCY_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(latency))); // in theory, this could overflow, but no one has latency that high

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_SENT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(client->GetTotalNetworkPacketsSent())));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_RECEIVED_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(client->GetTotalNetworkPacketsReceived())));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_COUNT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(client->GetRetransmitCount())));    // need to ensure we don't overflow here with uint32_t

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_REQUEST_COUNT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(client->GetRetransmitRequestCount())));     // need to ensure we don't overflow here with uint32_t

        return clientObject;
    }
}

//
// Response Object Payload
// {
//   "clients" :
//   [
//     {
//       "entryIdentifier" : "some guid",
//       "enabled" : true,
//       "sessionActive" : true,
//       "remoteAddress" : "ip or host",
//       "remotePort" : "port number",
//       "localPort" : "port number",
//       "endpointDeviceId" : "id of associated ump endpoint",
//       "createMidi1Ports" : true,
//       "fallbackMidi1PortCount" : 1
//      },
//     ...
//   ]
// }
// 
//
_Use_decl_annotations_
HRESULT 
CMidi2NetworkMidiConfigurationManager::RunCommandEnumerateClients(
    json::JsonObject& responseObject) noexcept
try
{
    json::JsonArray clientsArray;

    // Driven by the configured definitions rather than the live clients, so an entry which is
    // not currently connected still appears. A direct connection which gave up would otherwise
    // vanish from the list entirely.
    for (auto const& definition : TransportState::Current().GetClientDefinitions())
    {
        clientsArray.Append(BuildEnumeratedClientObject(
            definition,
            TransportState::Current().GetClient(definition.EntryIdentifier)));
    }


    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY, clientsArray);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;

}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}



//
// Response Object Payload
// {
//   "hosts" :
//   [
//     {
//       "entryIdentifier" : "some guid",
//       "enabled" : true,
//       "hasStarted" : true,
//       "actualPort" : "12345",
//       "name" : "Advertised Endpoint Name",
//       "productInstanceId" : "instance id",
//       "createMidi1Ports" : true,
//       "fallbackMidi1PortCount" : 1,
//       "serviceInstanceName" : "foobarbaz"
//      },
//     ...
//   ]
// }
// 
//

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandGetTransportSettings(
    json::JsonObject& responseObject) noexcept
try
{
    auto const& settings = TransportState::Current().TransportSettings;

    json::JsonObject settingsObject;

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY,
        json::JsonValue::CreateNumberValue(settings.ForwardErrorCorrectionMaxCommandPacketCount));

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY,
        json::JsonValue::CreateNumberValue(settings.RetransmitBufferMaxCommandPacketCount));

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(settings.OutboundPingInterval)));

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY,
        json::JsonValue::CreateNumberValue(settings.MaxHostConnections));

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(settings.InvitationPendingTimeout)));

    settingsObject.SetNamedValue(
        MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(settings.DirectConnectionScanInterval)));

    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_RESPONSE_KEY, settingsObject);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


namespace
{
    // One remote client of a host, for enumerateHosts
    json::JsonObject BuildEnumeratedHostConnectionObject(
        _In_ std::shared_ptr<MidiNetworkHostConnection> const& connection,
        _In_ MidiNetworkRemoteClientIdentity const& identity)
    {
        json::JsonObject connectionObject;

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
            json::JsonValue::CreateStringValue(identity.UmpEndpointName));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
            json::JsonValue::CreateStringValue(identity.ProductInstanceId));

        auto remoteHostName = connection->GetRemoteHostName();

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(remoteHostName != nullptr ? remoteHostName.CanonicalName() : L""));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_PORT_KEY,
            json::JsonValue::CreateStringValue(connection->GetRemotePort()));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_SESSION_ACTIVE_KEY,
            json::JsonValue::CreateBooleanValue(connection->IsSessionActive()));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_PENDING_APPROVAL_KEY,
            json::JsonValue::CreateBooleanValue(connection->IsAwaitingUserApproval()));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_UMP_ENDPOINT_ID_KEY,
            json::JsonValue::CreateStringValue(connection->GetEndpointDeviceId()));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_CURRENT_LATENCY_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(connection->GetAndResetAverageLatencyTicks())));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_NETWORK_PACKETS_SENT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(connection->GetTotalNetworkPacketsSent())));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_NETWORK_PACKETS_RECEIVED_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(connection->GetTotalNetworkPacketsReceived())));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_RETRANSMIT_COUNT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(connection->GetRetransmitCount())));

        connectionObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_RETRANSMIT_REQUEST_COUNT_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(connection->GetRetransmitRequestCount())));

        return connectionObject;
    }

    // One host, for enumerateHosts: what was configured, what the running host reports, and the
    // remote clients reaching it.
    json::JsonObject BuildEnumeratedHostObject(
        _In_ std::shared_ptr<MidiNetworkHost> const& host,
        _In_ MidiNetworkHostDefinition const& definition)
    {
        json::JsonObject hostObject;

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY,
            json::JsonValue::CreateStringValue(winrt::hstring{ internal::GuidToString(definition.EntryIdentifier) }));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_IS_ENABLED_KEY,
            json::JsonValue::CreateBooleanValue(host->IsEnabled()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HAS_STARTED_KEY,
            json::JsonValue::CreateBooleanValue(host->HasStarted()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_PORT_KEY,
            json::JsonValue::CreateStringValue(host->ActualPort()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIGURED_PORT_KEY,
            json::JsonValue::CreateStringValue(
                definition.UseAutomaticPortAllocation ? MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO : definition.Port));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ALLOW_PORT_FALLBACK_KEY,
            json::JsonValue::CreateBooleanValue(definition.AllowPortFallback));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PORT_FALLBACK_USED_KEY,
            json::JsonValue::CreateBooleanValue(host->PortFallbackUsed()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NETWORK_ADAPTER_ID_KEY,
            json::JsonValue::CreateStringValue(winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(definition.NetworkAdapterId) }));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NETWORK_ADAPTER_NAME_KEY,
            json::JsonValue::CreateStringValue(definition.NetworkAdapterName));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY,
            json::JsonValue::CreateBooleanValue(definition.AllowNetworkAdapterFallback));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NETWORK_ADAPTER_MISSING_KEY,
            json::JsonValue::CreateBooleanValue(host->IsWaitingForNetworkAdapter() || host->NetworkAdapterFallbackUsed()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NETWORK_ADAPTER_FALLBACK_USED_KEY,
            json::JsonValue::CreateBooleanValue(host->NetworkAdapterFallbackUsed()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(host->ActualAddress()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY,
            json::JsonValue::CreateStringValue(definition.UmpEndpointName));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY,
            json::JsonValue::CreateStringValue(definition.ProductInstanceId));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(definition.CreateMidi1Ports));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_FALLBACK_MIDI1_PORT_COUNT_KEY,
            json::JsonValue::CreateNumberValue(definition.FallbackMidi1PortCount));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY,
            json::JsonValue::CreateStringValue(definition.ServiceInstanceName));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_SERVICE_INSTANCE_NAME_KEY,
            json::JsonValue::CreateStringValue(host->ActualServiceInstanceName()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_CHANGED_KEY,
            json::JsonValue::CreateBooleanValue(host->ServiceInstanceNameWasChanged()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_REMOTE_CLIENT_POLICY_KEY,
            json::JsonValue::CreateStringValue(
                definition.RemoteClientPolicy == MidiNetworkRemoteClientPolicy::PolicyRequireApproval ?
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL :
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));

        // Remote clients on this host, so an app polling every few seconds can show what is
        // connected and, more importantly, what is waiting on a user decision. Absence from this
        // list is how a caller learns a client went away; nothing tracks departures separately.
        json::JsonArray connectionsArray;

        for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(definition.EntryIdentifier))
        {
            if (connection == nullptr)
            {
                continue;
            }

            auto identity = connection->GetRemoteClientIdentity();

            // A remote which did not give both halves of its identity never gets an endpoint and
            // never reaches the approval gate, so it is not a connected device. Its connection
            // object lingers only until the idle reaper takes it. Listing it inflates the
            // connected count and offers the user a row which cannot be acted on: every verb
            // here addresses a remote by that identity pair.
            if (!identity.IsValid())
            {
                continue;
            }

            connectionsArray.Append(BuildEnumeratedHostConnectionObject(connection, identity));
        }

        hostObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONNECTIONS_ARRAY_KEY, connectionsArray);

        return hostObject;
    }
}

_Use_decl_annotations_
HRESULT 
CMidi2NetworkMidiConfigurationManager::RunCommandEnumerateHosts(
    json::JsonObject& responseObject) noexcept
try
{
    json::JsonArray hostsArray;

    for (auto const& host : TransportState::Current().GetHosts())
    {
        if (host == nullptr)
        {
            continue;
        }

        // The stored definition is the one configuration updates change. The host keeps the
        // copy it started with, and only falls back to it if the entry has just been removed.
        auto definition = TransportState::Current().GetHostDefinition(host->EntryIdentifier());

        hostsArray.Append(BuildEnumeratedHostObject(
            host,
            definition.has_value() ? *definition : host->GetDefinition()));
    }


    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY, hostsArray);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


namespace
{
    // Optional command arguments are simply absent rather than empty, so this keeps the call
    // sites readable.
    winrt::hstring OptionalCommandArgument(
        _In_ internal::MidiTransportCommandHelper& commandHelper,
        _In_ std::wstring const& key)
    {
        auto arg = commandHelper.Arguments()->find(key);

        return arg != commandHelper.Arguments()->end() ?
            winrt::hstring{ internal::TrimmedWStringCopy(arg->second) } :
            winrt::hstring{};
    }

    // Boolean command arguments arrive as text. An absent one means the caller did not say, so
    // it takes the default rather than reading as false and silently turning something off.
    bool OptionalCommandArgumentBool(
        _In_ internal::MidiTransportCommandHelper& commandHelper,
        _In_ std::wstring const& key,
        _In_ bool const defaultValue)
    {
        auto arg = commandHelper.Arguments()->find(key);

        if (arg == commandHelper.Arguments()->end())
        {
            return defaultValue;
        }

        auto const value = internal::ToLowerTrimmedWStringCopy(arg->second);

        return value == L"true" || value == L"1";
    }

    // Same idea for a numeric argument. Anything the caller cannot have meant, including text
    // which is not a number at all, falls back to the default rather than to zero.
    uint8_t OptionalCommandArgumentByte(
        _In_ internal::MidiTransportCommandHelper& commandHelper,
        _In_ std::wstring const& key,
        _In_ uint8_t const defaultValue,
        _In_ uint8_t const minimumValue,
        _In_ uint8_t const maximumValue)
    {
        auto arg = commandHelper.Arguments()->find(key);

        if (arg == commandHelper.Arguments()->end())
        {
            return defaultValue;
        }

        try
        {
            auto const value = std::stoi(internal::TrimmedWStringCopy(arg->second));

            if (value < minimumValue || value > maximumValue)
            {
                return defaultValue;
            }

            return static_cast<uint8_t>(value);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    // FILETIME to ISO 8601 UTC, with the full 100ns resolution so the value round-trips. An
    // unset time returns empty rather than a 1601 date, which would read as a real answer.
    std::wstring PendingRequestTimeToString(uint64_t const fileTime)
    {
        if (fileTime == 0)
        {
            return {};
        }

        FILETIME ft{};
        ft.dwLowDateTime = static_cast<DWORD>(fileTime & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>(fileTime >> 32);

        SYSTEMTIME st{};

        if (!FileTimeToSystemTime(&ft, &st))
        {
            return {};
        }

        wchar_t buffer[40]{};

        if (swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02u.%07uZ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            static_cast<uint32_t>(fileTime % 10000000)) < 0)
        {
            return {};
        }

        return std::wstring{ buffer };
    }
}

// Returns a list of all remote connections/invites that are pending
// some sort of action, like user approval.
//
// Response Object Payload
// {
//   "pendingRemoteClients" :
//   [
//     {
//       "entryIdentifier" : "host entry id, pass back to approveRemoteClient/denyRemoteClient",
//       "umpEndpointName" : "remote's own name, also an approval argument",
//       "productInstanceId" : "remote's own id, also an approval argument",
//       "hostUmpEndpointName" : "which of this PC's hosts is being asked",
//       "hostServiceInstanceName" : "service instance name of that host",
//       "remoteAddress" : "ip the invitation arrived from, for display",
//       "requestTime" : "2026-08-12T01:23:45.6789012Z"
//      },
//     ...
//   ]
// }
//
// The array is empty when nothing is waiting, which is the normal case for a poll. There is no
// separate "nothing changed" reply: a caller compares against what it last saw.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandGetPendingRemoteClients(
    json::JsonObject& responseObject) noexcept
try
{
    json::JsonArray clientsArray;

    // Pending clients belong to a host, and only the host role has an approval gate at all, so
    // outbound clients are not walked here.
    for (auto const host : TransportState::Current().GetHosts())
    {
        auto def = host->GetDefinition();

        for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(def.EntryIdentifier))
        {
            if (connection == nullptr || !connection->IsAwaitingUserApproval())
            {
                continue;
            }

            json::JsonObject clientObject;

            auto identity = connection->GetRemoteClientIdentity();

            // These three are the approveRemoteClient / denyRemoteClient arguments, under the
            // key names those commands already read, so an entry goes back unmodified.
            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                json::JsonValue::CreateStringValue(winrt::hstring{ internal::GuidToString(def.EntryIdentifier) }));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                json::JsonValue::CreateStringValue(identity.UmpEndpointName));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                json::JsonValue::CreateStringValue(identity.ProductInstanceId));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_NAME_KEY,
                json::JsonValue::CreateStringValue(def.UmpEndpointName));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_SERVICE_INSTANCE_NAME_KEY,
                json::JsonValue::CreateStringValue(def.ServiceInstanceName));

            auto remoteHostName = connection->GetRemoteHostName();

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY,
                json::JsonValue::CreateStringValue(remoteHostName != nullptr ? remoteHostName.CanonicalName() : L""));

            clientObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_REQUEST_TIME_KEY,
                json::JsonValue::CreateStringValue(PendingRequestTimeToString(connection->GetUserApprovalRequestedFileTime())));

            clientsArray.Append(clientObject);
        }
    }

    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENTS_RESPONSE_ARRAY_KEY, clientsArray);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


namespace
{
    // Only for an argument the command table has already required, so it is known to be there
    std::wstring const& RequiredCommandArgument(
        _In_ internal::MidiTransportCommandHelper& commandHelper,
        _In_ std::wstring const& key)
    {
        return commandHelper.Arguments()->at(key);
    }

    // An entry identifier which is not a GUID doesn't fail the command. It is reported in the
    // response, which the caller still reads, and false tells the command to stop there.
    bool TryReadEntryIdentifierArgument(
        _In_ internal::MidiTransportCommandHelper& commandHelper,
        _In_ std::wstring const& key,
        _Inout_ json::JsonObject& responseObject,
        _Out_ winrt::guid& entryIdentifier)
    {
        if (TryParseEntryIdentifier(winrt::hstring{ RequiredCommandArgument(commandHelper, key) }, entryIdentifier))
        {
            return true;
        }

        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_INVALID_ENTRY_IDENTIFIER, internal::ResourceGetWString(IDS_ERROR_INVALID_ENTRY_IDENTIFIER));

        return false;
    }

    // Which remote client an approval, denial, disconnect or forget is about
    MidiNetworkRemoteClientIdentity RemoteClientIdentityArgument(
        _In_ internal::MidiTransportCommandHelper& commandHelper)
    {
        MidiNetworkRemoteClientIdentity identity{};

        identity.UmpEndpointName = internal::TrimmedWStringCopy(RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY));
        identity.ProductInstanceId = internal::TrimmedWStringCopy(RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY));

        return identity;
    }

    std::map<std::wstring, bool> NetworkTransportCapabilities()
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, true);

        // MIDI 1.0 port naming is not wired up for this transport yet
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_PORTS, false);

        // These are the generic per-endpoint verbs, which this transport does not implement. It
        // exposes network-specific verbs instead (start-host, stop-host, connect-direct,
        // disconnect-client), because starting a host and connecting a client are not the same
        // operation and cannot share one endpoint-level verb.
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RESTART_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_DISCONNECT_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RECONNECT_ENDPOINT, false);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_TRANSPORT_SETTINGS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_MDNS, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT, true);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT, true);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS, true);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_CAPABILITY_CUSTOM_ENDPOINT_NAME_ON_CREATE, true);

        return capabilities;
    }
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ProcessCommand(
    json::JsonObject const& transportObject,
    json::JsonObject& responseObject) noexcept
try
{
    using CommandHelper = internal::MidiTransportCommandHelper;
    using Self = CMidi2NetworkMidiConfigurationManager;

    struct NetworkCommand
    {
        std::wstring_view Verb;

        // A command missing any of these fails with E_INVALIDARG
        std::vector<std::wstring_view> RequiredArguments;

        HRESULT(*Run)(Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject);
    };

    static auto const runHostCommand = [](
        CommandHelper& commandHelper,
        json::JsonObject& responseObject,
        auto&& run) -> HRESULT
        {
            winrt::guid hostEntryIdentifier{};

            if (!TryReadEntryIdentifierArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER, responseObject, hostEntryIdentifier))
            {
                return S_OK;
            }

            return run(hostEntryIdentifier);
        };

    static auto const runRemoteClientDecision = [](
        Self& self,
        CommandHelper& commandHelper,
        json::JsonObject& responseObject,
        bool const approve) -> HRESULT
        {
            auto const scope = internal::ToLowerTrimmedWStringCopy(
                RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE));

            // Only "always" is written down. "once" authorizes the waiting connection and
            // nothing else, and "untilRestart" is a memory-only denial by definition.
            bool const persist = (scope == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ALWAYS);

            auto const identity = RemoteClientIdentityArgument(commandHelper);

            return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                {
                    return self.RunCommandRemoteClientDecision(hostEntryIdentifier, identity, approve, persist, responseObject);
                });
        };

    static std::vector<NetworkCommand> const commands
    {
        {
            MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES, { },
            [](Self&, CommandHelper&, json::JsonObject& responseObject) -> HRESULT
            {
                auto capabilities = NetworkTransportCapabilities();

                internal::SetConfigurationResponseObjectSuccess(responseObject);
                internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);

                return S_OK;
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS, { },
            [](Self& self, CommandHelper&, json::JsonObject& responseObject) -> HRESULT
            {
                return self.RunCommandEnumerateClients(responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS, { },
            [](Self& self, CommandHelper&, json::JsonObject& responseObject) -> HRESULT
            {
                return self.RunCommandGetPendingRemoteClients(responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS, { },
            [](Self& self, CommandHelper&, json::JsonObject& responseObject) -> HRESULT
            {
                return self.RunCommandEnumerateHosts(responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_TRANSPORT_SETTINGS, { },
            [](Self& self, CommandHelper&, json::JsonObject& responseObject) -> HRESULT
            {
                return self.RunCommandGetTransportSettings(responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST,
            { MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                    {
                        return self.RunCommandStartHost(hostEntryIdentifier, responseObject);
                    });
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST,
            { MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                    {
                        return self.RunCommandStopHost(hostEntryIdentifier, responseObject);
                    });
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST,
            { MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                    {
                        return self.RunCommandRemoveHost(hostEntryIdentifier, responseObject);
                    });
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                winrt::guid clientEntryIdentifier{};

                if (!TryReadEntryIdentifierArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, responseObject, clientEntryIdentifier))
                {
                    return S_OK;
                }

                return self.RunCommandConnectDirect(
                    clientEntryIdentifier,
                    RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS).c_str(),
                    RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT).c_str(),
                    RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME).c_str(),
                    OptionalCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY),
                    OptionalCommandArgumentBool(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT),
                    OptionalCommandArgumentByte(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM),
                    responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_MDNS,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_MATCH_ID,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                winrt::guid clientEntryIdentifier{};

                if (!TryReadEntryIdentifierArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, responseObject, clientEntryIdentifier))
                {
                    return S_OK;
                }

                return self.RunCommandConnectMdns(
                    clientEntryIdentifier,
                    winrt::hstring{ internal::TrimmedWStringCopy(RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_MATCH_ID)) },
                    RequiredCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME).c_str(),
                    OptionalCommandArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY),
                    OptionalCommandArgumentBool(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT),
                    OptionalCommandArgumentByte(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM, MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM),
                    responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT,
            { MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                winrt::guid clientEntryIdentifier{};

                if (!TryReadEntryIdentifierArgument(commandHelper, MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, responseObject, clientEntryIdentifier))
                {
                    return S_OK;
                }

                return self.RunCommandDisconnectClient(clientEntryIdentifier, responseObject);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                return runRemoteClientDecision(self, commandHelper, responseObject, true);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                return runRemoteClientDecision(self, commandHelper, responseObject, false);
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_REMOTE_CLIENT,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                auto const identity = RemoteClientIdentityArgument(commandHelper);

                return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                    {
                        return self.RunCommandDisconnectRemoteClient(hostEntryIdentifier, identity, responseObject);
                    });
            }
        },
        {
            MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT,
            {
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY
            },
            [](Self& self, CommandHelper& commandHelper, json::JsonObject& responseObject) -> HRESULT
            {
                auto const identity = RemoteClientIdentityArgument(commandHelper);

                return runHostCommand(commandHelper, responseObject, [&](winrt::guid const& hostEntryIdentifier)
                    {
                        return self.RunCommandForgetRemoteClient(hostEntryIdentifier, identity, responseObject);
                    });
            }
        },
    };

    auto commandHelper = CommandHelper::ParseCommand(transportObject);
    auto const verb = commandHelper.Command();

    if (verb.empty())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_MISSING_COMMAND, internal::ResourceGetWString(IDS_ERROR_MISSING_COMMAND));

        // we S_OK this because the response object is valid and should be read
        return S_OK;
    }

    auto const command = std::find_if(
        commands.begin(),
        commands.end(),
        [&verb](NetworkCommand const& candidate)
        {
            return candidate.Verb == verb;
        });

    if (command == commands.end())
    {
        internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, NETWORK_ERROR_CODE_UNRECOGNIZED_COMMAND, internal::ResourceGetWString(IDS_ERROR_UNRECOGNIZED_COMMAND));

        return S_OK;
    }

    for (auto const& requiredArgument : command->RequiredArguments)
    {
        if (commandHelper.Arguments()->find(std::wstring{ requiredArgument }) == commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }

    RETURN_IF_FAILED(command->Run(*this, commandHelper, responseObject));

    // we return S_OK no matter what, so the response object will be parsed
    return S_OK;
}
catch (...)
{
    // noexcept, so an escaping exception would terminate the service rather than fail the
    // command. The JSON response builders below can all throw on a low memory condition.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception running command", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


//"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}":
//{
//    "_comment": "Network MIDI 2.0",
//    "transportSettings" :
//    {
//        "maxForwardErrorCorrectionCommandPackets": 2,
//        "maxRetransmitBufferCommandPackets": 50,
//        "outboundPingInterval": 2000,
//        "directConnectionScanInterval": 20000,
//        "maxHostConnections": 64,
//        "invitationPendingTimeout": 120000
//    },
//    "create":
//    {
//        "hosts":
//        {
//            "{090ad480-3cf8-4228-b58f-469f773e4b61}":
//            {
//                "name": "Windows MIDI Services Host",
//                "serviceInstanceName": "windows",
//                "productInstanceId": "3263827-5150Net2Preview",
//                "networkProtocol": "udp",
//                "port": "auto",
//                "enabled": true,
//                "advertise": true,
//                "authentication": "none",
//                "remoteClientPolicy": "requireApproval",
//                "allowedClients":
//                [
//                    { "umpEndpointName": "BomeBox", "productInstanceId": "CC851C0080257A96" }
//                ],
//                "deniedClients": []
//            }
//        },
//        "clients":
//        {
//            "{25d5789f-c84d-4310-91ea-bdc1680f35d5}":
//            {
//                "_comment": "kissbox",
//                "networkProtocol" : "udp",
//                "match" :
//                {
//                    "id": "DnsSd#kb7C5D0A_1._midi2._udp.local#0"
//                }
//            },
//            "{ba0f1174-b343-4b32-84e4-01e368d08545}":
//            {
//                "_comment": "direct ip example",
//                "networkProtocol" : "udp",
//                "match" :
//                {
//                    "directHostNameOrIP": "192.168.1.243",
//                    "directPort": "39820"
//                }
//            },
//            "{fd0bf1d0-4ac6-4d57-b0e8-7bb29b029f4f}":
//            {
//                "_comment": "direct host name example",
//                "networkProtocol" : "udp",
//                "match" :
//                {
//                    "directHostNameOrIP": "BomeBox.local",
//                    "directPort": "51492"
//                }
//            }
//        }
//    }
//},
//


namespace
{
    // The settings which mean the same thing whether an entry is being made or changed. Create
    // calls these on a freshly built definition, so a key the entry does not mention takes the
    // definition's own default; update calls them on the definition already held, so the same
    // absent key leaves the current value alone. Both paths going through one function is what
    // stops a setting being creatable but not updatable.
    //
    // Anything not here is deliberately fixed once the entry exists. The match criteria, the
    // identity, the port, the advertisement and the authentication settings all decide how the
    // connection is built in the first place, and honoring them in an update would record a
    // change which nothing acts on until the entry is rebuilt. Changing one of those means
    // removing the entry and adding it back. A host's network adapter is the exception: the
    // endpoint worker moves a running host when it changes, the same way it does when the
    // adapter itself comes or goes.
    void ApplyEntrySettings(
        _In_ json::JsonObject const& entry,
        _Inout_ MidiNetworkClientDefinition& definition) noexcept
    {
        definition.CustomEndpointName = internal::TrimmedHStringCopy(
            SafeGetNamedString(
                entry,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY,
                definition.CustomEndpointName));

        definition.CreateMidi1Ports = SafeGetNamedBoolean(
            entry,
            MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
            definition.CreateMidi1Ports);

        definition.FallbackMidi1PortCount = SafeGetNamedByte(
            entry,
            MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
            definition.FallbackMidi1PortCount,
            MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
            MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);
    }

    void ApplyEntrySettings(
        _In_ json::JsonObject const& entry,
        _Inout_ MidiNetworkHostDefinition& definition) noexcept
    {
        definition.CustomEndpointName = internal::TrimmedHStringCopy(
            SafeGetNamedString(
                entry,
                MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY,
                definition.CustomEndpointName));

        definition.CreateMidi1Ports = SafeGetNamedBoolean(
            entry,
            MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
            definition.CreateMidi1Ports);

        definition.FallbackMidi1PortCount = SafeGetNamedByte(
            entry,
            MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
            definition.FallbackMidi1PortCount,
            MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
            MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);

        // An empty id is every adapter. One which is not a GUID is ignored.
        GUID networkAdapterId{};

        if (::WindowsMidiServicesInternal::TryParseMidiNetworkAdapterId(
                std::wstring{ SafeGetNamedString(
                    entry,
                    MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_ID_KEY,
                    winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(definition.NetworkAdapterId) }) },
                networkAdapterId))
        {
            definition.NetworkAdapterId = networkAdapterId;
        }

        std::wstring networkAdapterName{ internal::TrimmedHStringCopy(
            SafeGetNamedString(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_NAME_KEY, definition.NetworkAdapterName)) };

        if (networkAdapterName.size() > MIDI_NETWORK_ADAPTER_NAME_MAX_CHARS) networkAdapterName.resize(MIDI_NETWORK_ADAPTER_NAME_MAX_CHARS);

        definition.NetworkAdapterName = winrt::hstring{ networkAdapterName };

        std::wstring physicalAddress{ internal::TrimmedHStringCopy(
            SafeGetNamedString(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_PHYSICAL_ADDRESS_KEY, definition.NetworkAdapterPhysicalAddress)) };

        if (physicalAddress.size() > MIDI_NETWORK_ADAPTER_PHYSICAL_ADDRESS_MAX_CHARS) physicalAddress.clear();

        definition.NetworkAdapterPhysicalAddress = winrt::hstring{ physicalAddress };

        definition.AllowNetworkAdapterFallback = SafeGetNamedBoolean(
            entry,
            MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY,
            definition.AllowNetworkAdapterFallback);
    }
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ProcessEntryUpdates(
    json::JsonObject const& updateSection,
    json::JsonObject& responseObject) noexcept
try
{
    UNREFERENCED_PARAMETER(responseObject);

    auto endpointManager = TransportState::Current().GetEndpointManager();

    // Applies one entry's worth of changes. The count is the only thing which can take effect
    // without rebuilding the endpoint, so it is the only thing pushed to a live one. Whether an
    // endpoint has MIDI 1.0 ports at all is fixed when the endpoint is created, so the flag is
    // recorded for the next connection and nothing is torn down to act on it.
    auto applyToDefinition = [&endpointManager](
        json::JsonObject const& entry,
        auto& definition,
        std::vector<std::wstring> const& liveEndpointDeviceIds)
    {
        auto const previousCount = definition.FallbackMidi1PortCount;

        ApplyEntrySettings(entry, definition);

        if (definition.FallbackMidi1PortCount == previousCount)
        {
            return;
        }

        if (endpointManager == nullptr)
        {
            return;
        }

        for (auto const& endpointDeviceId : liveEndpointDeviceIds)
        {
            if (endpointDeviceId.empty()) continue;

            LOG_IF_FAILED(endpointManager->RefreshMidi1PortsForEndpoint(endpointDeviceId, definition.FallbackMidi1PortCount));
        }
    };

    auto hostsObject = SafeGetNamedObject(updateSection, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY);

    if (hostsObject != nullptr && hostsObject.Size() > 0)
    {
        for (auto const& it = hostsObject.First(); it.HasCurrent(); it.MoveNext())
        {
            winrt::guid entryIdentifier{};

            if (!TryParseEntryIdentifier(it.Current().Key(), entryIdentifier)) continue;

            auto entry = SafeGetNamedObject(hostsObject, it.Current().Key());

            if (entry == nullptr) continue;

            // A host's endpoints belong to the remote clients connected to it, so every one of
            // them follows the host's setting.
            std::vector<std::wstring> endpointDeviceIds{};

            for (auto const& connection : TransportState::Current().GetHostConnectionsForHost(entryIdentifier))
            {
                if (connection != nullptr)
                {
                    endpointDeviceIds.push_back(connection->GetEndpointDeviceId());
                }
            }

            auto definition = TransportState::Current().GetHostDefinition(entryIdentifier);

            if (!definition.has_value()) continue;

            // Changed on a copy and written back, so no other thread reads a half-applied update
            applyToDefinition(entry, *definition, endpointDeviceIds);

            LOG_IF_FAILED(TransportState::Current().UpdateHostDefinition(
                entryIdentifier,
                [&definition](MidiNetworkHostDefinition& stored)
                {
                    stored.CustomEndpointName = definition->CustomEndpointName;
                    stored.CreateMidi1Ports = definition->CreateMidi1Ports;
                    stored.FallbackMidi1PortCount = definition->FallbackMidi1PortCount;
                    stored.NetworkAdapterId = definition->NetworkAdapterId;
                    stored.NetworkAdapterName = definition->NetworkAdapterName;
                    stored.NetworkAdapterPhysicalAddress = definition->NetworkAdapterPhysicalAddress;
                    stored.AllowNetworkAdapterFallback = definition->AllowNetworkAdapterFallback;
                }));

            // A running host has its own copy, taken when it started
            if (auto host = TransportState::Current().GetHost(entryIdentifier); host != nullptr)
            {
                host->SetFallbackMidi1PortCount(definition->FallbackMidi1PortCount);
                host->SetCreateMidi1Ports(definition->CreateMidi1Ports);

                host->SetNetworkAdapter(
                    definition->NetworkAdapterId,
                    definition->NetworkAdapterName,
                    definition->NetworkAdapterPhysicalAddress,
                    definition->AllowNetworkAdapterFallback);

                // Moves the host if the change means it belongs somewhere else. A host which is
                // already where it should be is left alone, so its connections stay up.
                if (endpointManager != nullptr)
                {
                    endpointManager->RequestNetworkAdapterReconcile();
                }
            }
        }
    }

    auto clientsObject = SafeGetNamedObject(updateSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY);

    if (clientsObject != nullptr && clientsObject.Size() > 0)
    {
        for (auto const& it = clientsObject.First(); it.HasCurrent(); it.MoveNext())
        {
            winrt::guid entryIdentifier{};

            if (!TryParseEntryIdentifier(it.Current().Key(), entryIdentifier)) continue;

            auto entry = SafeGetNamedObject(clientsObject, it.Current().Key());

            if (entry == nullptr) continue;

            std::vector<std::wstring> endpointDeviceIds{};

            for (auto const& connection : TransportState::Current().GetAllNetworkConnectionsForClient(entryIdentifier))
            {
                if (connection != nullptr)
                {
                    endpointDeviceIds.push_back(connection->GetEndpointDeviceId());
                }
            }

            // The definitions are what the endpoint creator builds from, and what survives a
            // reconnect, so they are updated whether or not a connection is up right now.
            auto definition = TransportState::Current().GetClientDefinition(entryIdentifier);

            if (!definition.has_value()) continue;

            applyToDefinition(entry, *definition, endpointDeviceIds);

            LOG_IF_FAILED(TransportState::Current().UpdateClientDefinition(
                entryIdentifier,
                [&definition](MidiNetworkClientDefinition& stored)
                {
                    stored.CustomEndpointName = definition->CustomEndpointName;
                    stored.CreateMidi1Ports = definition->CreateMidi1Ports;
                    stored.FallbackMidi1PortCount = definition->FallbackMidi1PortCount;
                }));

            if (auto client = TransportState::Current().GetClient(entryIdentifier); client != nullptr)
            {
                client->SetFallbackMidi1PortCount(definition->FallbackMidi1PortCount);
            }
        }
    }

    return S_OK;
}
catch (...)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception processing entry updates", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_FAIL;
}


_Use_decl_annotations_
void
CMidi2NetworkMidiConfigurationManager::ApplyTransportSettings(
    json::JsonObject const& transportSettingsSection)
{
    bool anySettingAdjusted{ false };

    auto const fecPackets = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY,
        MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT,
        MIDI_NETWORK_FEC_PACKET_COUNT_LOWER_BOUND, MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND,
        anySettingAdjusted);

    auto const retransmitBuffer = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY,
        MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT,
        MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_LOWER_BOUND, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_UPPER_BOUND,
        anySettingAdjusted);

    auto const outboundPingInterval = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY,
        MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT,
        MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_UPPER_BOUND,
        anySettingAdjusted);

    auto const maxHostConnections = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY,
        MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT,
        MIDI_NETWORK_HOST_MAX_CONNECTIONS_LOWER_BOUND, MIDI_NETWORK_HOST_MAX_CONNECTIONS_ABSOLUTE_MAX,
        anySettingAdjusted);

    auto const invitationPendingTimeout = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY,
        MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT,
        MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_LOWER_BOUND, MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_UPPER_BOUND,
        anySettingAdjusted);

    auto const directConnectionScanInterval = ReadClampedTransportSetting(
        transportSettingsSection, MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY,
        MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT,
        MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_LOWER_BOUND, MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_UPPER_BOUND,
        anySettingAdjusted);

    // A bad value is corrected rather than rejected: refusing the whole command would take
    // the user's hosts and clients down with it over a mistyped number.
    if (anySettingAdjusted)
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"One or more transport settings were missing, the wrong type, or out of range, and have been corrected", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(fecPackets, "fec packets"),
            TraceLoggingUInt32(retransmitBuffer, "retransmit buffer"),
            TraceLoggingUInt32(outboundPingInterval, "ping interval"),
            TraceLoggingUInt32(maxHostConnections, "max host connections"),
            TraceLoggingUInt32(invitationPendingTimeout, "invitation pending timeout"),
            TraceLoggingUInt32(directConnectionScanInterval, "direct connection scan interval")
        );
    }

    TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount = static_cast<uint8_t>(fecPackets);
    TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount = static_cast<uint16_t>(retransmitBuffer);
    TransportState::Current().TransportSettings.OutboundPingInterval = outboundPingInterval;
    TransportState::Current().TransportSettings.MaxHostConnections = static_cast<uint16_t>(maxHostConnections);
    TransportState::Current().TransportSettings.InvitationPendingTimeout = invitationPendingTimeout;
    TransportState::Current().TransportSettings.DirectConnectionScanInterval = directConnectionScanInterval;
}

// GetComputerName reports the length without the terminator, and the buffer is longer than that.
// Resized to fit, or the name carries trailing nulls into every place it is used.
static winrt::hstring GetComputerNameOrEmpty()
{
    std::wstring buffer{};
    DWORD bufferSize = MAX_COMPUTERNAME_LENGTH + 1;
    buffer.resize(bufferSize);

    if (!GetComputerName(buffer.data(), &bufferSize))
    {
        return winrt::hstring{ };
    }

    buffer.resize(bufferSize);

    return winrt::hstring{ buffer };
}

_Use_decl_annotations_
bool
CMidi2NetworkMidiConfigurationManager::TryReadHostDefinition(
    winrt::hstring const& entryKey,
    json::JsonObject const& hostEntry,
    MidiNetworkHostDefinition& definition,
    winrt::hstring& errorMessage,
    uint32_t& errorCode)
{
    definition = MidiNetworkHostDefinition{ };
    errorMessage = winrt::hstring{ };
    errorCode = NETWORK_ERROR_CODE_UNKNOWN_ERROR;

    if (hostEntry == nullptr)
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_PARSING_JSON);
        errorCode = NETWORK_ERROR_CODE_INVALID_JSON;
        return false;
    }

    // currently, UDP is the only allowed protocol
    auto protocol = internal::ToLowerTrimmedHStringCopy(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

    if (protocol != MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_NETWORK_PROTOCOL);
        errorCode = NETWORK_ERROR_CODE_INVALID_NETWORK_PROTOCOL;
        return false;
    }

    if (!TryParseEntryIdentifier(internal::TrimmedHStringCopy(entryKey), definition.EntryIdentifier))
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_ENTRY_IDENTIFIER);
        errorCode = NETWORK_ERROR_CODE_INVALID_ENTRY_IDENTIFIER;
        return false;
    }

    definition.IsEnabled = SafeGetNamedBoolean(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
    definition.Advertise = SafeGetNamedBoolean(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);

    // The settings which can also be changed later are read the one way both paths
    // read them. The definition is new, so an absent key takes its default.
    ApplyEntrySettings(hostEntry, definition);

    definition.UmpEndpointName = internal::TrimmedHStringCopy(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L""));
    definition.ProductInstanceId = internal::TrimmedHStringCopy(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY, L""));

    if (definition.ProductInstanceId.empty())
    {
        definition.ProductInstanceId = winrt::hstring{ TransportState::Current().GetEffectiveProductInstanceId() };
    }

    definition.Port = internal::TrimmedHStringCopy(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));
    definition.AllowPortFallback = SafeGetNamedBoolean(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_PORT_FALLBACK_KEY, true);

    definition.Authentication = MidiNetworkHostAuthenticationFromJsonString(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE));
    definition.RemoteClientPolicy = MidiNetworkRemoteClientPolicyFromJsonString(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));

    ReadRemoteClientIdentityList(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, definition.AllowedClientKeys);
    ReadRemoteClientIdentityList(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, definition.DeniedClientKeys);

    // An empty service instance name defaults to the machine name
    definition.ServiceInstanceName = internal::TrimmedHStringCopy(SafeGetNamedString(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY, L""));

    if (definition.ServiceInstanceName.empty())
    {
        definition.ServiceInstanceName = GetComputerNameOrEmpty();
    }

    // The name becomes the DNS-SD instance and the virtual parent device id, so a
    // second host claiming it cannot work. This used to be an unimplemented TODO, and
    // the collision surfaced much later as a host which started but could never
    // create an endpoint.
    if (TransportState::Current().IsHostServiceInstanceNameInUse(
            std::wstring{ definition.ServiceInstanceName },
            definition.EntryIdentifier))
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_SERVICE_INSTANCE_NAME_IN_USE);
        errorCode = NETWORK_ERROR_CODE_SERVICE_INSTANCE_NAME_IN_USE;
        return false;
    }

    // A host's .local name, for its DNS-SD registration. Which adapters it runs on is the
    // networkAdapterId setting, read with the other settings an update can change.
    for (auto const& host : winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames())
    {
        if ((host.Type() == HostNameType::DomainName) &&
            (host.RawName().ends_with(L".local")))
        {
            definition.HostName = host.RawName();
            break;
        }
    }

    if (definition.Port == MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO ||
        definition.Port == L"" ||
        definition.Port == L"0")
    {
        // this will cause us to use an auto-generated free port
        definition.Port = L"";
        definition.UseAutomaticPortAllocation = true;
    }
    else
    {
        definition.UseAutomaticPortAllocation = false;
    }

    // Two hosts cannot share a port. Checked against what each started host is
    // actually bound to, so this also catches a manual port colliding with one the
    // system handed out automatically. A host waiting for an automatic port has no
    // number yet and cannot conflict.
    if (!definition.UseAutomaticPortAllocation &&
        TransportState::Current().IsHostPortInUse(
            std::wstring{ definition.Port },
            definition.EntryIdentifier))
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_HOST_PORT_IN_USE);
        errorCode = NETWORK_ERROR_CODE_HOST_PORT_IN_USE;
        return false;
    }

    return SUCCEEDED(ValidateHostDefinition(definition, errorMessage, errorCode));
}

_Use_decl_annotations_
bool
CMidi2NetworkMidiConfigurationManager::TryReadClientDefinition(
    winrt::hstring const& entryKey,
    json::JsonObject const& clientEntry,
    MidiNetworkClientDefinition& definition,
    winrt::hstring& errorMessage,
    uint32_t& errorCode)
{
    definition = MidiNetworkClientDefinition{ };
    errorMessage = winrt::hstring{ };
    errorCode = NETWORK_ERROR_CODE_UNKNOWN_ERROR;

    if (clientEntry == nullptr)
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_PARSING_JSON);
        errorCode = NETWORK_ERROR_CODE_INVALID_JSON;
        return false;
    }

    // currently, UDP is the only allowed protocol
    auto protocol = internal::ToLowerTrimmedHStringCopy(SafeGetNamedString(clientEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

    if (protocol != MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_NETWORK_PROTOCOL);
        errorCode = NETWORK_ERROR_CODE_INVALID_NETWORK_PROTOCOL;
        return false;
    }

    if (!TryParseEntryIdentifier(internal::TrimmedHStringCopy(entryKey), definition.EntryIdentifier))
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_ENTRY_IDENTIFIER);
        errorCode = NETWORK_ERROR_CODE_INVALID_ENTRY_IDENTIFIER;
        return false;
    }

    definition.Enabled = SafeGetNamedBoolean(clientEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);

    // The settings which can also be changed later are read the one way both
    // paths read them. The definition is new, so an absent key takes its default.
    ApplyEntrySettings(clientEntry, definition);

    // TODO: Add ability for config file to specify the localEndpointName and localProductInstanceId
    definition.LocalEndpointName = GetComputerNameOrEmpty();

    // TODO: we may want to provide the local product instance id as a system-wide setting. Same with name
    definition.LocalProductInstanceId = winrt::hstring{ TransportState::Current().GetEffectiveProductInstanceId() };

    auto matchSection = SafeGetNamedObject(clientEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY);

    if (!matchSection)
    {
        // we have no way to match against endpoints, so this is a failure
        errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_MATCH_ENTRY);
        errorCode = NETWORK_ERROR_CODE_MISSING_MATCH_ENTRY;
        return false;
    }

    // for the moment, we only match on the actual device id, so must be mdns-advertised
    definition.MatchId = internal::TrimmedHStringCopy(SafeGetNamedString(matchSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY, L""));

    // direct connection properties
    definition.MatchDirectHostNameOrIPAddress = internal::TrimmedHStringCopy(SafeGetNamedString(matchSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY, L""));
    definition.MatchDirectPort = internal::TrimmedHStringCopy(SafeGetNamedString(matchSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY, L""));

    definition.MatchProductInstanceId = internal::TrimmedHStringCopy(SafeGetNamedString(matchSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID_KEY, L""));
    definition.MatchUmpEndpointName = internal::TrimmedHStringCopy(SafeGetNamedString(matchSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY, L""));

    return true;
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
try
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // if we're passed a null or empty json, we just quietly exit
    if (configurationJsonSection == nullptr) return S_OK;


    //OutputDebugString(L"JSON Received by CMidi2NetworkMidiConfigurationManager");
    //OutputDebugString(configurationJsonSection);
    //OutputDebugString(L"\n");


    json::JsonObject jsonObject;
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    json::JsonArray createdDevicesResponseArray;

    auto jsonFalse = json::JsonValue::CreateBooleanValue(false);
    auto jsonTrue = json::JsonValue::CreateBooleanValue(true);

    // Assume failure
    responseObject.SetNamedValue(
        MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
        jsonFalse);


    if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
    {
        TraceLoggingWrite(
            MidiNetworkMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(configurationJsonSection, "json")
        );

        internal::JsonStringifyObjectToOutParam(responseObject, response);

        RETURN_IF_FAILED(E_INVALIDARG);
    }

    // command. If there's a command in the payload, we ignore anything else
    if (internal::MidiTransportCommandHelper::TransportObjectContainsCommand(jsonObject))
    {
        auto hr = ProcessCommand(jsonObject, responseObject);

        internal::JsonStringifyObjectToOutParam(responseObject, response);

        return hr;
    }



    auto transportSettingsSection = SafeGetNamedObject(jsonObject, MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY);

    auto createSection = SafeGetNamedObject(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);
    auto updateSection = SafeGetNamedObject(jsonObject, MIDI_CONFIG_JSON_NETWORK_MIDI_UPDATE_ENTRIES_KEY);
    auto removeSection = SafeGetNamedObject(jsonObject, MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY);

    // Endpoint customization, in the array form every other transport uses. Kept separate from
    // the object-shaped "update" below, which is for this transport's own host and client
    // entries. Done before the create section because that one hands definitions to the endpoint
    // creator thread, and an endpoint built before this cache is filled would miss its name.
    LOG_IF_FAILED(ProcessEndpointCustomizations(jsonObject, responseObject));

    // transport-global settings

    // TODO: Move these to registry instead?

    if (transportSettingsSection != nullptr && transportSettingsSection.Size() > 0)
    {
        ApplyTransportSettings(transportSettingsSection);

        // A settings-only update has now done everything it was asked to do. Without this the
        // response still says failure, because success is otherwise only set while creating.
        internal::SetConfigurationResponseObjectSuccess(responseObject);
    }

    // "create" entries
    if (createSection != nullptr && createSection.Size() > 0)
    {
        auto hostsSection = SafeGetNamedObject(createSection, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY);
        auto clientsSection = SafeGetNamedObject(createSection, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY);

        // An invalid entry is skipped rather than aborting the whole update, because earlier
        // entries have already been added by the time we find it. The first failure is what
        // gets reported back, so the response is a failure if any entry was rejected.
        bool anyEntryFailed{ false };
        bool anyEntryAdded{ false };
        winrt::hstring firstEntryErrorMessage{ };
        uint32_t firstEntryErrorCode{ 0 };

        auto reportEntryFailure = [&](winrt::hstring const& entryIdentifier, winrt::hstring const& message, uint32_t const errorCode)
            {
                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Invalid configuration entry. Entry skipped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(entryIdentifier.c_str(), "entry identifier"),
                    TraceLoggingWideString(message.c_str(), "error"),
                    TraceLoggingUInt32(errorCode, "error code")
                );

                if (!anyEntryFailed)
                {
                    anyEntryFailed = true;
                    firstEntryErrorMessage = message;
                    firstEntryErrorCode = errorCode;
                }
            };

        // we typically have one host, but a user may create more than one if needed.
        // External clients connect to the host, which has a single port and IP address
        if (hostsSection != nullptr && hostsSection.Size() > 0)
        {
            for (auto const& it = hostsSection.First(); it.HasCurrent(); it.MoveNext())
            {
                MidiNetworkHostDefinition definition{ };
                winrt::hstring errorMessage{ };
                uint32_t errorCode{ NETWORK_ERROR_CODE_UNKNOWN_ERROR };

                if (!TryReadHostDefinition(
                        it.Current().Key(),
                        SafeGetNamedObject(hostsSection, it.Current().Key()),
                        definition,
                        errorMessage,
                        errorCode))
                {
                    reportEntryFailure(it.Current().Key(), errorMessage, errorCode);
                    continue;
                }

                TraceLoggingWrite(
                    MidiNetworkMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Host definition validated. Creating host", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                LOG_IF_FAILED(TransportState::Current().AddHostDefinition(definition));

                anyEntryAdded = true;

                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                    jsonTrue);
            }
        }

        // clients are connections to external devices made by the service. Each
        // connection goes to a unique combination of IP and port on an external
        // device (or application)
        if (clientsSection != nullptr && clientsSection.Size() > 0)
        {
            for (auto const& it = clientsSection.First(); it.HasCurrent(); it.MoveNext())
            {
                MidiNetworkClientDefinition definition{ };
                winrt::hstring errorMessage{ };
                uint32_t errorCode{ NETWORK_ERROR_CODE_UNKNOWN_ERROR };

                if (!TryReadClientDefinition(
                        it.Current().Key(),
                        SafeGetNamedObject(clientsSection, it.Current().Key()),
                        definition,
                        errorMessage,
                        errorCode))
                {
                    reportEntryFailure(it.Current().Key(), errorMessage, errorCode);
                    continue;
                }

                LOG_IF_FAILED(TransportState::Current().AddClientDefinition(definition));

                anyEntryAdded = true;

                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                    jsonTrue);
            }
        }

        // Pending definitions are turned into live hosts and connections by the endpoint
        // manager's creator thread, which otherwise only wakes on its scan interval. Without
        // this, creating a host through the API reports success and then does nothing visible
        // for up to DirectConnectionScanInterval.
        if (anyEntryAdded)
        {
            auto endpointManager = TransportState::Current().GetEndpointManager();

            if (endpointManager != nullptr)
            {
                LOG_IF_FAILED(endpointManager->WakeupBackgroundEndpointCreatorThread());
            }
        }

        if (anyEntryFailed)
        {
            internal::SetConfigurationResponseObjectFailWithErrorCode(responseObject, firstEntryErrorCode, std::wstring{ firstEntryErrorMessage });
        }
    }

    // "update" entries
    if (updateSection != nullptr && updateSection.Size() > 0)
    {
        if (SUCCEEDED(ProcessEntryUpdates(updateSection, responseObject)))
        {
            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
    }

    // "remove" entries
    if (removeSection != nullptr && removeSection.Size() > 0)
    {
        // Endpoint customizations are the only thing removed this way. Taking a host or a client
        // away has to stop what is running as well as forget the entry, so each is a command of
        // its own: removeHost and disconnectClient. A "remove" section naming a host or a client
        // is the configuration file's record of the entry being gone, and the SDK applies that to
        // the file when it saves, which is why nothing is expected here for them.
        LOG_IF_FAILED(ProcessEndpointCustomizationRemovals(removeSection, responseObject));
    }












    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(responseObject.Stringify().c_str())
    );

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, response);

    return S_OK;
}
catch (winrt::hresult_error const& ex)
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exception processing the configuration payload", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(ex.message().c_str(), "error"),
        TraceLoggingHResult(ex.code(), MIDI_TRACE_EVENT_HRESULT_FIELD)
    );

    return ex.code();
}
catch (std::bad_alloc const&)
{
    return E_OUTOFMEMORY;
}
catch (...)
{
    // Deliberately not wil::ResultFromCaughtException here. This is the outermost handler on a
    // COM boundary fed attacker-shaped JSON, and WIL fail-fasts on an exception type it does
    // not recognize, which would turn the guard into the crash.
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_ERROR,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Unrecognized exception processing the configuration payload", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_UNEXPECTED;
}


HRESULT
CMidi2NetworkMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );




    return S_OK;
}

