// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace winrt::Windows::Networking;

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

// Reads an array of { umpEndpointName, productInstanceId } objects into comparison keys.
// Entries missing either field cannot identify a device, so they are skipped rather than
// stored as something which would match nothing or, worse, everything.
void
ReadRemoteClientIdentityList(
    _In_ json::JsonObject const& parent,
    _In_ std::wstring const& arrayKey,
    _Inout_ std::vector<std::wstring>& keys)
{
    if (!parent.HasKey(arrayKey))
    {
        return;
    }

    auto entries = parent.GetNamedArray(arrayKey, nullptr);

    if (entries == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < entries.Size(); i++)
    {
        auto entry = entries.GetObjectAt(i);

        if (entry == nullptr)
        {
            continue;
        }

        MidiNetworkRemoteClientIdentity identity{};

        identity.UmpEndpointName = internal::TrimmedWStringCopy(std::wstring{ entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L"") });
        identity.ProductInstanceId = internal::TrimmedWStringCopy(std::wstring{ entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L"") });

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
    winrt::hstring& errorMessage)
{
    errorMessage = L"";

    // is there a unique identifier?

    if (definition.EntryIdentifier.empty())
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER);
        return E_INVALIDARG;
    }

    // TODO: was that identifier already in use?


    if (definition.UmpEndpointName.empty())
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_NAME);
        return E_INVALIDARG;
    }

    if (definition.ProductInstanceId.empty())
    {
        errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_PRODUCT_INSTANCE_ID);
        return E_INVALIDARG;
    }

    // validate user authentication
    if (definition.Authentication != MidiNetworkHostAuthentication::NoAuthentication)
    {
        if (definition.AuthenticationCredentialIdentifier.empty())
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_MISSING_CREDENTIAL_IDENTIFIER);
            return E_INVALIDARG;
        }

        MidiNetworkCredentialIdentifier identifier{ std::wstring{ definition.AuthenticationCredentialIdentifier } };

        if (!identifier.IsWellFormed())
        {
            errorMessage = internal::ResourceGetHString(IDS_ERROR_INVALID_CREDENTIAL_IDENTIFIER);
            return E_INVALIDARG;
        }

        // Refused rather than silently downgraded. Accepting unauthenticated invitations on a
        // host the user asked to protect would be worse than refusing to start it.
        // https://github.com/microsoft/MIDI/issues/733
        errorMessage = internal::ResourceGetHString(IDS_ERROR_AUTHENTICATION_NOT_IMPLEMENTED);
        return E_NOTIMPL;
    }

    return S_OK;

}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandStopHost(
    winrt::hstring const& hostEntryId,
    json::JsonObject& responseObject) noexcept
{
    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    if (SUCCEEDED(host->Stop()))
    {
        internal::SetConfigurationResponseObjectSuccess(responseObject);
        return S_OK;
    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_STOP_HOST));
        return S_OK;
    }
}

// Stops the host and removes the entry. stopHost deliberately keeps the entry so the host can be
// started again, which also means it keeps holding its service instance name. This is the verb
// which gives that name back, and the only way to be rid of a host without restarting the
// service.
_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandRemoveHost(
    winrt::hstring const& hostEntryId,
    json::JsonObject& responseObject) noexcept
{
    // Detached first, so nothing can start it again while it is being shut down, and so the
    // state lock is not held across a Shutdown which blocks on the network.
    auto host = TransportState::Current().RemoveHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    // Sends the Byes, tears down the connections, unbinds the port and removes the parent device.
    LOG_IF_FAILED(host->Shutdown());

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandStartHost(
    winrt::hstring const& hostEntryId,
    json::JsonObject& responseObject) noexcept
{
    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
        return S_OK;
    }

    auto startHr = host->Start();

    if (SUCCEEDED(startHr))
    {
        internal::SetConfigurationResponseObjectSuccess(responseObject);
        return S_OK;
    }
    else
    {
        // the HRESULT goes in the response error code rather than the message, so the message
        // stays localizable
        internal::SetConfigurationResponseObjectFailWithErrorCode(
            responseObject,
            static_cast<uint32_t>(startHr),
            internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_START_HOST));

        return S_OK;
    }
}



_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandConnectDirect(
    winrt::hstring const& configEntryId,
    winrt::hstring const& remoteAddress,
    winrt::hstring const& remotePort,
    winrt::hstring const& umpEndpointName,
    json::JsonObject& responseObject) noexcept
{

    if (remoteAddress.empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_ADDRESS));
        return S_OK;
    }

    if (remotePort.empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_PORT));
        return S_OK;
    }

    // do some validation of the remote port
    uint16_t remotePortNumeric{0};
    auto num = wcstol(remotePort.c_str(), NULL, 10);
    if (num > WORD_MAX || num < 1)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_INVALID_REMOTE_PORT));
        return S_OK;
    }
    else
    {
        remotePortNumeric = static_cast<uint16_t>(num);
    }

    // this happens all in real-time, unlike the stuff that is done via the config file

    auto clientDefinition = std::make_shared<MidiNetworkClientDefinition>();

    // TODO: These should be parameters
    clientDefinition->CreateMidi1Ports = true;
    clientDefinition->EntryIdentifier = configEntryId;
    clientDefinition->MatchDirectHostNameOrIPAddress = remoteAddress;
    clientDefinition->MatchDirectPort = remotePort;
    clientDefinition->LocalEndpointName = umpEndpointName;

    auto endpointManager = TransportState::Current().GetEndpointManager();

    RETURN_HR_IF_NULL(E_UNEXPECTED, endpointManager);

    endpointManager->StartNewClient(
        clientDefinition, 
        remoteAddress,
        remotePortNumeric);


    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::RunCommandDisconnectClient(
    winrt::hstring const& configEntryId,
    json::JsonObject& responseObject) noexcept
{

    if (configEntryId.empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }


    auto client = TransportState::Current().GetClient(configEntryId);

    if (client != nullptr)
    {
        LOG_IF_FAILED(client->DisconnectByUser());
    }

    TransportState::Current().RemoveClient(configEntryId);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
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
    winrt::hstring const& hostEntryId,
    MidiNetworkRemoteClientIdentity const& identity,
    bool const approve,
    bool const persist,
    json::JsonObject& responseObject) noexcept
{
    if (hostEntryId.empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_ENTRY_IDENTIFIER));
        return S_OK;
    }

    if (!identity.IsValid())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));
        return S_OK;
    }

    auto host = TransportState::Current().GetHost(hostEntryId);

    if (host == nullptr)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_HOST_NOT_FOUND));
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

    // Release, or refuse, whatever is parked for this identity. There can be more than one if the
    // client retried from a new port while the user was deciding.
    auto key = identity.Key();

    for (auto const& connection : TransportState::Current().GetAllNetworkConnectionsForHost(hostEntryId))
    {
        if (connection == nullptr || !connection->IsAwaitingUserApproval())
        {
            continue;
        }

        if (connection->GetRemoteClientIdentity().Key() != key)
        {
            continue;
        }

        if (approve)
        {
            LOG_IF_FAILED(connection->ApproveByUser());
        }
        else
        {
            LOG_IF_FAILED(connection->DenyByUser());
        }
    }

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
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
//       "createMidi1Ports" : true
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
{
    json::JsonArray clientsArray;

    for (auto const client : TransportState::Current().GetClients())
    {
        json::JsonObject clientObject;

        auto def = client->GetDefinition();

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY,
            json::JsonValue::CreateStringValue(def.EntryIdentifier));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_MDNS_MATCH_ID_KEY,
            json::JsonValue::CreateStringValue(def.MatchId));

        //clientObject.SetNamedValue(
        //    MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_ENABLED_KEY,
        //    json::JsonValue::CreateBooleanValue(client->IsEnabled()));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_SESSION_ACTIVE_KEY,
            json::JsonValue::CreateBooleanValue(client->IsSessionActive()));

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

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(def.CreateMidi1Ports));

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

        clientsArray.Append(clientObject);
    }


    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY, clientsArray);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;

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
//       "serviceInstanceName" : "foobarbaz"
//      },
//     ...
//   ]
// }
// 
//

_Use_decl_annotations_
HRESULT 
CMidi2NetworkMidiConfigurationManager::RunCommandEnumerateHosts(
    json::JsonObject& responseObject) noexcept
{
    json::JsonArray hostsArray;

    for (auto const host : TransportState::Current().GetHosts())
    {
        json::JsonObject hostObject;

        auto def = host->GetDefinition();

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY,
            json::JsonValue::CreateStringValue(def.EntryIdentifier));

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
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY,
            json::JsonValue::CreateStringValue(host->ActualAddress()));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY,
            json::JsonValue::CreateStringValue(def.UmpEndpointName));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY,
            json::JsonValue::CreateStringValue(def.ProductInstanceId));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(def.CreateMidi1Ports));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY,
            json::JsonValue::CreateStringValue(def.ServiceInstanceName));

        hostObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_REMOTE_CLIENT_POLICY_KEY,
            json::JsonValue::CreateStringValue(
                def.RemoteClientPolicy == MidiNetworkRemoteClientPolicy::PolicyRequireApproval ?
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL :
                MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));

        // Remote clients on this host, so an app polling every few seconds can show what is
        // connected and, more importantly, what is waiting on a user decision. Absence from this
        // list is how a caller learns a client went away; nothing tracks departures separately.
        json::JsonArray connectionsArray;

        for (auto const& connection : TransportState::Current().GetAllNetworkConnectionsForHost(def.EntryIdentifier))
        {
            if (connection == nullptr)
            {
                continue;
            }

            json::JsonObject connectionObject;

            auto identity = connection->GetRemoteClientIdentity();

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

            connectionsArray.Append(connectionObject);
        }

        hostObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONNECTIONS_ARRAY_KEY, connectionsArray);

        hostsArray.Append(hostObject);
    }


    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY, hostsArray);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}


namespace
{
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
{
    json::JsonArray clientsArray;

    // Pending clients belong to a host, and only the host role has an approval gate at all, so
    // outbound clients are not walked here.
    for (auto const host : TransportState::Current().GetHosts())
    {
        auto def = host->GetDefinition();

        for (auto const& connection : TransportState::Current().GetAllNetworkConnectionsForHost(def.EntryIdentifier))
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
                json::JsonValue::CreateStringValue(def.EntryIdentifier));

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


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ProcessCommand(
    json::JsonObject const& transportObject,
    json::JsonObject& responseObject) noexcept
{
    auto commandHelper = internal::MidiTransportCommandHelper::ParseCommand(transportObject);

    if (commandHelper.Command().empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_MISSING_COMMAND));

        // we S_OK this because the response object is valid and should be read
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES)
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, false);
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
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT, true);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT, true);

        capabilities.emplace(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS, true);


        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS)
    {
        RETURN_IF_FAILED(RunCommandEnumerateClients(responseObject));
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS)
    {
        RETURN_IF_FAILED(RunCommandGetPendingRemoteClients(responseObject));
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS)
    {
        RETURN_IF_FAILED(RunCommandEnumerateHosts(responseObject));
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST)
    {
        winrt::hstring hostEntryIdentifier{};

        auto arg = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER);

        if (arg != commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(RunCommandStartHost(arg->second.c_str(), responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST)
    {
        auto arg = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER);

        if (arg != commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(RunCommandStopHost(arg->second.c_str(), responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST)
    {
        auto arg = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER);

        if (arg != commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(RunCommandRemoveHost(arg->second.c_str(), responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT)
    {
        auto entryId = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER);
        auto addr = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS);
        auto port = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT);
        auto name = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME);

        if (entryId != commandHelper.Arguments()->end() &&
            addr != commandHelper.Arguments()->end() &&
            port != commandHelper.Arguments()->end() &&
            name != commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(RunCommandConnectDirect(
                entryId->second.c_str(), 
                addr->second.c_str(), 
                port->second.c_str(), 
                name->second.c_str(),
                responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT)
    {
        auto entryId = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER);

        if (entryId != commandHelper.Arguments()->end())
        {
            RETURN_IF_FAILED(RunCommandDisconnectClient(
                entryId->second.c_str(),
                responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT ||
             commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT)
    {
        bool const approve = (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT);

        auto hostEntryId = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER);
        auto name = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY);
        auto productInstanceId = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY);
        auto scope = commandHelper.Arguments()->find(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE);

        if (hostEntryId != commandHelper.Arguments()->end() &&
            name != commandHelper.Arguments()->end() &&
            productInstanceId != commandHelper.Arguments()->end() &&
            scope != commandHelper.Arguments()->end())
        {
            auto scopeValue = internal::ToLowerTrimmedWStringCopy(scope->second);

            // Only "always" is written down. "once" authorizes the waiting connection and
            // nothing else, and "untilRestart" is a memory-only denial by definition.
            bool const persist = (scopeValue == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ALWAYS);

            MidiNetworkRemoteClientIdentity identity{};

            identity.UmpEndpointName = internal::TrimmedWStringCopy(name->second);
            identity.ProductInstanceId = internal::TrimmedWStringCopy(productInstanceId->second);

            RETURN_IF_FAILED(RunCommandRemoteClientDecision(
                hostEntryId->second.c_str(),
                identity,
                approve,
                persist,
                responseObject));
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }
    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, internal::ResourceGetWString(IDS_ERROR_UNRECOGNIZED_COMMAND));
    }

    // we return S_OK no matter what, so the response object will be parsed
    return S_OK;
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
//        "invitationPendingTimeout": 120000,
//        "productInstanceId": "optional machine-wide identity, shared by hosts and clients"
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


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
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



    auto transportSettingsSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY, nullptr);

    auto createSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);
    auto updateSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);
    auto removeSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, nullptr);

    // transport-global settings

    // TODO: Move these to registry instead?

    if (transportSettingsSection != nullptr && transportSettingsSection.Size() > 0)
    {
        uint32_t fecPackets{ };
        uint32_t retransmitBuffer{};
        uint32_t outboundPingInterval{};
        uint32_t maxHostConnections{};
        uint32_t invitationPendingTimeout{};
        uint32_t directConnectionScanInterval{};

        fecPackets = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY, MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT));
        retransmitBuffer = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT));
        outboundPingInterval = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT));
        maxHostConnections = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY, MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT));
        invitationPendingTimeout = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY, MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT));
        directConnectionScanInterval = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY, MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT));

        // Validate the above values. Out-of-range values are reported rather than quietly
        // clamped, so a user who mistypes a setting finds out instead of wondering why it had
        // no effect.
        std::wstring settingsErrorMessage{ };
        if (fecPackets < MIDI_NETWORK_FEC_PACKET_COUNT_LOWER_BOUND || fecPackets > MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_FEC_PACKET_COUNT_OUT_OF_RANGE);
        }
        else if (retransmitBuffer < MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_LOWER_BOUND || retransmitBuffer > MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_UPPER_BOUND)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_RETRANSMIT_BUFFER_SIZE_OUT_OF_RANGE);
        }
        else if (outboundPingInterval < MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND || outboundPingInterval > MIDI_NETWORK_OUTBOUND_PING_INTERVAL_UPPER_BOUND)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_PING_INTERVAL_OUT_OF_RANGE);
        }
        else if (maxHostConnections < MIDI_NETWORK_HOST_MAX_CONNECTIONS_LOWER_BOUND || maxHostConnections > MIDI_NETWORK_HOST_MAX_CONNECTIONS_ABSOLUTE_MAX)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_MAX_HOST_CONNECTIONS_OUT_OF_RANGE);
        }
        else if (invitationPendingTimeout < MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_LOWER_BOUND || invitationPendingTimeout > MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_UPPER_BOUND)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_INVITATION_PENDING_TIMEOUT_OUT_OF_RANGE);
        }
        else if (directConnectionScanInterval < MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_LOWER_BOUND || directConnectionScanInterval > MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_UPPER_BOUND)
        {
            settingsErrorMessage = internal::ResourceGetWString(IDS_ERROR_DIRECT_CONNECTION_SCAN_INTERVAL_OUT_OF_RANGE);
        }

        if (!settingsErrorMessage.empty())
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invalid transport setting in configuration", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(settingsErrorMessage.c_str(), "error"),
                TraceLoggingUInt32(fecPackets, "fec packets"),
                TraceLoggingUInt32(retransmitBuffer, "retransmit buffer"),
                TraceLoggingUInt32(outboundPingInterval, "ping interval"),
                TraceLoggingUInt32(maxHostConnections, "max host connections"),
                TraceLoggingUInt32(invitationPendingTimeout, "invitation pending timeout")
            );

            internal::SetConfigurationResponseObjectFail(responseObject, settingsErrorMessage);
            internal::JsonStringifyObjectToOutParam(responseObject, response);

            RETURN_IF_FAILED(E_INVALIDARG);
        }

        TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount = static_cast<uint8_t>(fecPackets);
        TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount = static_cast<uint16_t>(retransmitBuffer);
        TransportState::Current().TransportSettings.OutboundPingInterval = static_cast<uint32_t>(outboundPingInterval);
        TransportState::Current().TransportSettings.MaxHostConnections = static_cast<uint16_t>(maxHostConnections);
        TransportState::Current().TransportSettings.InvitationPendingTimeout = invitationPendingTimeout;
        TransportState::Current().TransportSettings.DirectConnectionScanInterval = directConnectionScanInterval;

        // A settings-only update has now done everything it was asked to do. Without this the
        // response still says failure, because success is otherwise only set while creating.
        internal::SetConfigurationResponseObjectSuccess(responseObject);

        // Optional. Left empty here means TransportState supplies the machine-derived default.
        TransportState::Current().TransportSettings.ProductInstanceId = internal::TrimmedWStringCopy(
            std::wstring{ transportSettingsSection.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_MACHINE_PRODUCT_INSTANCE_ID_KEY, L"") });
    }

    // "create" entries
    if (createSection != nullptr && createSection.Size() > 0)
    {
        auto hostsSection = createSection.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, nullptr);
        auto clientsSection = createSection.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, nullptr);

        // An invalid entry is skipped rather than aborting the whole update, because earlier
        // entries have already been added by the time we find it. The first failure is what
        // gets reported back, so the response is a failure if any entry was rejected.
        bool anyEntryFailed{ false };
        winrt::hstring firstEntryErrorMessage{ };
        uint32_t firstEntryErrorCode{ 0 };

        auto reportEntryFailure = [&](winrt::hstring const& entryIdentifier, winrt::hstring const& message, HRESULT const errorCode)
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
                    TraceLoggingHResult(errorCode, MIDI_TRACE_EVENT_HRESULT_FIELD)
                );

                if (!anyEntryFailed)
                {
                    anyEntryFailed = true;
                    firstEntryErrorMessage = message;
                    firstEntryErrorCode = static_cast<uint32_t>(errorCode);
                }
            };

        // we typically have one host, but a user may create more than one if needed.
        // External clients connect to the host, which has a single port and IP address
        if (hostsSection != nullptr && hostsSection.Size() > 0)
        {
            for (auto const& it = hostsSection.First(); it.HasCurrent(); it.MoveNext())
            {
                auto hostEntry = hostsSection.GetNamedObject(it.Current().Key());

                auto definition = std::make_shared<MidiNetworkHostDefinition>();
                RETURN_IF_NULL_ALLOC(definition);

                winrt::hstring validationErrorMessage{ };

                // currently, UDP is the only allowed protocol
                auto protocol = internal::ToLowerTrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

                if (protocol != MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
                {
                    reportEntryFailure(it.Current().Key(), internal::ResourceGetHString(IDS_ERROR_INVALID_NETWORK_PROTOCOL), E_INVALIDARG);
                    continue;
                }

                definition->EntryIdentifier = internal::TrimmedHStringCopy(it.Current().Key());

                definition->IsEnabled = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
                definition->Advertise = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);

                definition->CreateMidi1Ports = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT);

                definition->UmpEndpointName = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L""));
                definition->ProductInstanceId = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY, L""));

                if (definition->ProductInstanceId.empty())
                {
                    definition->ProductInstanceId = winrt::hstring{ TransportState::Current().GetEffectiveProductInstanceId() };
                }

                definition->Port = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));

                definition->Authentication = MidiNetworkHostAuthenticationFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE));
                definition->RemoteClientPolicy = MidiNetworkRemoteClientPolicyFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY));

                ReadRemoteClientIdentityList(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, definition->AllowedClientKeys);
                ReadRemoteClientIdentityList(hostEntry, MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, definition->DeniedClientKeys);

                // read authentication information
                if (definition->Authentication != MidiNetworkHostAuthentication::NoAuthentication)
                {
                    // Only the identifier ever reaches the service. The secret itself is stored
                    // by the Settings app. See MidiNetworkCredentials.h for the open questions
                    // on where that store lives and how the service reads it.
                    if (definition->Authentication == MidiNetworkHostAuthentication::PasswordAuthentication)
                    {
                        definition->AuthenticationCredentialIdentifier = internal::TrimmedHStringCopy(
                            hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_GLOBAL_PASSWORD_KEY, L""));
                    }
                    else if (definition->Authentication == MidiNetworkHostAuthentication::UserAuthentication)
                    {
                        definition->AuthenticationCredentialIdentifier = internal::TrimmedHStringCopy(
                            hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_USER_AUTH_KEY, L""));
                    }
                }


                // generate host name and other info

                auto serviceInstanceNamePrefix = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY, L""));

                // if the provided service instance name is empty, default to 
                // machine name. If that name is already in use, add an additional
                // disambiguation value
                if (serviceInstanceNamePrefix.empty())
                {
                    std::wstring buffer{};
                    DWORD bufferSize = MAX_COMPUTERNAME_LENGTH + 1;
                    buffer.resize(bufferSize);

                    bool validName = GetComputerName(buffer.data(), &bufferSize);
                    if (validName)
                    {
                        serviceInstanceNamePrefix = buffer;
                    }
                }

                definition->ServiceInstanceName = serviceInstanceNamePrefix;

                // The name becomes the DNS-SD instance and the virtual parent device id, so a
                // second host claiming it cannot work. This used to be an unimplemented TODO, and
                // the collision surfaced much later as a host which started but could never
                // create an endpoint.
                if (TransportState::Current().IsHostServiceInstanceNameInUse(
                        std::wstring{ definition->ServiceInstanceName },
                        std::wstring{ definition->EntryIdentifier }))
                {
                    reportEntryFailure(
                        it.Current().Key(),
                        internal::ResourceGetHString(IDS_ERROR_SERVICE_INSTANCE_NAME_IN_USE),
                        HRESULT_FROM_WIN32(ERROR_DUP_NAME));

                    continue;
                }

                //definition.HostName = definition.ServiceInstanceName + L"._midi2._udp.local";

                // TODO: User should be able to specify the adapter, host name, etc.

                // TODO: This should be pulled out of the loop
                auto hostNames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();

                for (auto const& host : hostNames)
                {
                    if ((host.Type() == HostNameType::DomainName) &&
                        (host.RawName().ends_with(L".local")))
                    {
                        definition->HostName = host.RawName();
                        break;
                    }
                }


                if (definition->Port == MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO ||
                    definition->Port == L"" ||
                    definition->Port == L"0")
                {
                    // this will cause us to use an auto-generated free port
                    definition->Port = L"";
                    definition->UseAutomaticPortAllocation = true;
                }
                else
                {
                    definition->UseAutomaticPortAllocation = false;
                }


                // validate the entry

                if (SUCCEEDED(ValidateHostDefinition(*definition, validationErrorMessage)))
                {
                    TraceLoggingWrite(
                        MidiNetworkMidiTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Host definition validated. Creating host", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    // create the host definition

                    // add to our collection of hosts
                    TransportState::Current().AddPendingHostDefinition(definition);

                    responseObject.SetNamedValue(
                        MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                        jsonTrue);
                }
                else
                {
                    reportEntryFailure(it.Current().Key(), validationErrorMessage, E_INVALIDARG);
                }
            }

        }

        // clients are connections to external devices made by the service. Each
        // connection goes to a unique combination of IP and port on an external
        // device (or application)
        if (clientsSection != nullptr && clientsSection.Size() > 0)
        {
            for (auto const& it = clientsSection.First(); it.HasCurrent(); it.MoveNext())
            {
                auto clientEntry = clientsSection.GetNamedObject(it.Current().Key());

                auto definition = std::make_shared<MidiNetworkClientDefinition>();
                RETURN_IF_NULL_ALLOC(definition);

                // currently, UDP is the only allowed protocol
                auto protocol = internal::ToLowerTrimmedHStringCopy(clientEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

                if (protocol != MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
                {
                    reportEntryFailure(it.Current().Key(), internal::ResourceGetHString(IDS_ERROR_INVALID_NETWORK_PROTOCOL), E_INVALIDARG);
                }
                else
                {
                    definition->EntryIdentifier = internal::TrimmedHStringCopy(it.Current().Key());
                    definition->Enabled = clientEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);

                    winrt::hstring localEndpointName{ };
                    winrt::hstring localProductInstanceId{ };

                    // TODO: Add ability for config file to specify the localEndpointName and localProductInstanceId
                    if (localEndpointName.empty())
                    {
                        std::wstring buffer{};
                        DWORD bufferSize = MAX_COMPUTERNAME_LENGTH + 1;
                        buffer.resize(bufferSize);

                        bool validName = GetComputerName(buffer.data(), &bufferSize);
                        if (validName)
                        {
                            localEndpointName = buffer;
                        }
                    }

                    // TODO: we may want to provide the local product instance id as a system-wide setting. Same with name
                    if (localProductInstanceId.empty())
                    {
                        localProductInstanceId = winrt::hstring{ TransportState::Current().GetEffectiveProductInstanceId() };
                    }

                    definition->LocalEndpointName = localEndpointName;
                    definition->LocalProductInstanceId = localProductInstanceId;

                    auto matchSection = clientEntry.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY, nullptr);

                    if (matchSection)
                    {
                        // for the moment, we only match on the actual device id, so must be mdns-advertised
                        definition->MatchId = internal::TrimmedHStringCopy(matchSection.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY, L""));

                        // direct connection properties
                        definition->MatchDirectHostNameOrIPAddress = internal::TrimmedHStringCopy(matchSection.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY, L""));
                        definition->MatchDirectPort = internal::TrimmedHStringCopy(matchSection.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY, L""));

                        // TODO: Validate port range, etc. and return an error if invalid


                        TransportState::Current().AddPendingClientDefinition(definition);

                        responseObject.SetNamedValue(
                            MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                            jsonTrue);
                    }
                    else
                    {
                        // we have no way to match against endpoints, so this is a failure
                        reportEntryFailure(it.Current().Key(), internal::ResourceGetHString(IDS_ERROR_MISSING_MATCH_ENTRY), E_INVALIDARG);
                    }
                }

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
        // this needs to allow for activating and deactivating existing entries, as well as setting the endpoint names and

    }

    // "remove" entries
    if (removeSection != nullptr && removeSection.Size() > 0)
    {
        // remove a host 





        // remove a connection to a remote host

    }














    //responseObject.SetNamedValue(
    //    MIDI_CONFIG_JSON_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY,
    //    createdDevicesResponseArray);


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

