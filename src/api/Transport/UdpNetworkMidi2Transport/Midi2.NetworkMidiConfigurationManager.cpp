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


MidiNetworkHostConnectionPolicy 
MidiNetworkHostConnectionPolicyFromJsonString(_In_ winrt::hstring const& jsonString)
{
    auto value = internal::ToLowerTrimmedHStringCopy(jsonString);

    if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY)
    {
        return MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections;
    }
    else if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_LIST)
    {
        return MidiNetworkHostConnectionPolicy::PolicyAllowFromIpList;
    }
    else if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_RANGE)
    {
        return MidiNetworkHostConnectionPolicy::PolicyAllowFromIpRange;
    }
    else
    {
        // default is any
        return MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections;
    }
}


// TODO: Move all strings to resources

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
        errorMessage = L"Missing Entry Identifier";
        return E_INVALIDARG;
    }

    // TODO: was that identifier already in use?


    if (definition.UmpEndpointName.empty())
    {
        errorMessage = L"Missing Endpoint Name";
        return E_INVALIDARG;
    }

    if (definition.ProductInstanceId.empty())
    {
        errorMessage = L"Missing Product Instance Id";
        return E_INVALIDARG;
    }

    //if (definition.ConnectionPolicy == MidiNetworkHostConnectionPolicy::PolicyAllowFromIpRange)
    //{
    //    // validate that there are exactly two entries

    //    if (definition.IpAddresses.size() != 2)
    //    {
    //        errorMessage = L"Connection policy IP address range needs exactly two valid entries to define the range.";
    //        return E_INVALIDARG;
    //    }
    //}
    //else if (definition.ConnectionPolicy == MidiNetworkHostConnectionPolicy::PolicyAllowFromIpList)
    //{
    //    // validate that there is at least one entry

    //    if (definition.IpAddresses.size() < 1)
    //    {
    //        errorMessage = L"Connection policy IP address list needs at least one valid entry.";
    //        return E_INVALIDARG;
    //    }

    //}


    // validate user authentication



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
        internal::SetConfigurationResponseObjectFail(responseObject, L"Host not found.");
        return S_OK;
    }

    if (SUCCEEDED(host->Stop()))
    {
        internal::SetConfigurationResponseObjectSuccess(responseObject);
        return S_OK;
    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Unable to stop host.");
        return S_OK;
    }
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
        internal::SetConfigurationResponseObjectFail(responseObject, L"Host not found.");
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
        auto error = L"Unable to start host. HRESULT: " + std::to_wstring(startHr);

        internal::SetConfigurationResponseObjectFail(responseObject, error);
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
        internal::SetConfigurationResponseObjectFail(responseObject, L"Missing remote address.");
        return S_OK;
    }

    if (remotePort.empty())
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Missing remote port.");
        return S_OK;
    }

    // do some validation of the remote port
    uint16_t remotePortNumeric{0};
    auto num = wcstol(remotePort.c_str(), NULL, 10);
    if (num > WORD_MAX || num < 1)
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Invalid remote port.");
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

    TransportState::Current().GetEndpointManager()->StartNewClient(
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
        internal::SetConfigurationResponseObjectFail(responseObject, L"Missing entry id.");
        return S_OK;
    }


    auto client = TransportState::Current().GetClient(configEntryId);

    if (client != nullptr)
    {
        LOG_IF_FAILED(client->Shutdown());
    }

    TransportState::Current().RemoveClient(configEntryId);

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

        //clientObject.SetNamedValue(
        //    MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_UMP_ENDPOINT_ID_KEY,
        //    json::JsonValue::CreateStringValue(client->UmpEndpointId));

        clientObject.SetNamedValue(
            MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CREATE_MIDI1_PORTS_KEY,
            json::JsonValue::CreateBooleanValue(def.CreateMidi1Ports));


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

        hostsArray.Append(hostObject);
    }


    responseObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY, hostsArray);

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
        internal::SetConfigurationResponseObjectFail(responseObject, L"Missing command.");

        // we S_OK this because the response object is valid and should be read
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_TRANSPORT_COMMAND_QUERY_CAPABILITIES)
    {
        std::map<std::wstring, bool> capabilities{};

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_PORTS, false);

        // revisit these once the functions are added in
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RESTART_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_DISCONNECT_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RECONNECT_ENDPOINT, false);

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);
    }
    else if (commandHelper.Command() == MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS)
    {
        RETURN_IF_FAILED(RunCommandEnumerateClients(responseObject));
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
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Unrecognized command.");
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
//        "outboundPingInterval": 2000.
//        "directConnectionScanInterval": 20000
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
//                "connectionPolicyIpv4": "allowAny"
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

        fecPackets = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY, MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT));
        retransmitBuffer = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT));
        outboundPingInterval = static_cast<uint32_t>(transportSettingsSection.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT));

        // Validate the above values

        fecPackets = max(fecPackets, MIDI_NETWORK_FEC_PACKET_COUNT_LOWER_BOUND);
        fecPackets = min(fecPackets, MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND);

        retransmitBuffer = max(retransmitBuffer, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_LOWER_BOUND);
        retransmitBuffer = min(retransmitBuffer, MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_UPPER_BOUND);

        outboundPingInterval = max(outboundPingInterval, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND);
        outboundPingInterval = min(outboundPingInterval, MIDI_NETWORK_OUTBOUND_PING_INTERVAL_UPPER_BOUND);

        TransportState::Current().TransportSettings.ForwardErrorCorrectionMaxCommandPacketCount = static_cast<uint8_t>(fecPackets);
        TransportState::Current().TransportSettings.RetransmitBufferMaxCommandPacketCount = static_cast<uint16_t>(retransmitBuffer);
        TransportState::Current().TransportSettings.OutboundPingInterval = static_cast<uint32_t>(outboundPingInterval);
    }

    // "create" entries
    if (createSection != nullptr && createSection.Size() > 0)
    {
        auto hostsSection = createSection.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, nullptr);
        auto clientsSection = createSection.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, nullptr);

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
                    validationErrorMessage = L"Invalid network protocol '" + protocol + L"' specified.";
                }

                definition->EntryIdentifier = internal::TrimmedHStringCopy(it.Current().Key());

                definition->IsEnabled = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
                definition->Advertise = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);

                definition->CreateMidi1Ports = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT);

                definition->UmpEndpointName = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L""));
                definition->ProductInstanceId = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY, L""));

                definition->Port = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));

                definition->Authentication = MidiNetworkHostAuthenticationFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE));
                definition->ConnectionPolicy = MidiNetworkHostConnectionPolicyFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY));

                // read the list of ip info
                //if (definition.ConnectionPolicy != MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections)
                //{
                //    auto addressArray = hostEntry.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_IPV4_ADDRESSES_KEY);

                //    for (uint32_t j = 0; j < addressArray.Size(); j++)
                //    {
                //        auto addressEntry = addressArray.GetStringAt(j);

                //        HostName address(addressEntry);

                //        definition.IpAddresses.push_back(address);
                //    }
                //}

                // read authentication information
                if (definition->Authentication != MidiNetworkHostAuthentication::NoAuthentication)
                {

                    if (definition->Authentication == MidiNetworkHostAuthentication::PasswordAuthentication)
                    {
                        // TODO: Read the password vault key
                    }
                    else if (definition->Authentication == MidiNetworkHostAuthentication::UserAuthentication)
                    {
                        // TODO: Read username/password vault key
                    }
                    else
                    {
                        // no authentication provided
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

                // TODO: See if the serviceInstanceName is already in use. If so, add a disambiguation number. Keep trying until unused one is found

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
                    // invalid entry
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

                winrt::hstring validationErrorMessage{ };

                // currently, UDP is the only allowed protocol
                auto protocol = internal::ToLowerTrimmedHStringCopy(clientEntry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

                if (protocol != MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
                {
                    validationErrorMessage = L"Invalid network protocol '" + protocol + L"' specified.";
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
                        localProductInstanceId = L"8675309-OU812";
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
                        validationErrorMessage = L"Missing match entry";
                    }
                }

            }
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

