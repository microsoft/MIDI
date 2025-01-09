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
    IMidiDeviceManagerInterface* midiDeviceManager,
    IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(transportId);
    UNREFERENCED_PARAMETER(midiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_midiDeviceManager));

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




//"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}":
//{
//    "_comment": "Network MIDI 2.0",
//    "transportSettings" :
//    {
//        "maxForwardErrorCorrectionCommandPackets": 2,
//            "maxRetransmitBufferCommandPackets" : 50,
//            "outboundPingInterval" : 2000
//    },
//    "create":
//    {
//        "hosts":
//        {
//            "{090ad480-3cf8-4228-b58f-469f773e4b61}":
//            {
//                "name": "Windows MIDI Services Host",
//                    "serviceInstanceName" : "windows",
//                    "productInstanceId" : "3263827-5150Net2Preview",
//                    "networkProtocol" : "udp",
//                    "port" : "auto",
//                    "enabled" : true,
//                    "advertise" : true,
//                    "authentication" : "none",
//                    "connectionPolicyIpv4" : "allowAny"
//            }
//        },
//        "clients":
//        {
//            "{25d5789f-c84d-4310-91ea-bdc1680f35d5}":
//            {
//                "_comment": "kissbox",
//                    "networkProtocol" : "udp",
//                    "match" :
//                {
//                    "id": "DnsSd#kb7C5D0A_1._midi2._udp.local#0"
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

    auto transportSettingsSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY, nullptr);

    auto createSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);
    auto updateSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);
    auto removeSection = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, nullptr);

    // transport-global settings
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

                definition->Enabled = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY, true);
                definition->Advertise = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);

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

        }

    }

    // "update" entries
    if (updateSection != nullptr && updateSection.Size() > 0)
    {

    }

    // "remove" entries
    if (removeSection != nullptr && removeSection.Size() > 0)
    {

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

