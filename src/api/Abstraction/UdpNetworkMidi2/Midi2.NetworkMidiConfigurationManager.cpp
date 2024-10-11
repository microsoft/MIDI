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
    IUnknown* midiDeviceManager,
    IUnknown* midiServiceConfigurationManagerInterface
)
{
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

    m_transportId = transportId;

    return S_OK;
}



MidiNetworkUdpHostAuthentication MidiNetworkUdpHostAuthenticationFromJsonString(_In_ winrt::hstring const& jsonString)
{
    auto value = internal::ToLowerTrimmedHStringCopy(jsonString);

    if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE)
    {
        return MidiNetworkUdpHostAuthentication::NoAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD)
    {
        return MidiNetworkUdpHostAuthentication::PasswordAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER)
    {
        return MidiNetworkUdpHostAuthentication::UserAuthentication;
    }
    else
    {
        // default is any
        return MidiNetworkUdpHostAuthentication::NoAuthentication;
    }
}


MidiNetworkUdpHostConnectionPolicy MidiNetworkUdpHostConnectionPolicyFromJsonString(_In_ winrt::hstring const& jsonString)
{
    auto value = internal::ToLowerTrimmedHStringCopy(jsonString);

    if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY)
    {
        return MidiNetworkUdpHostConnectionPolicy::AllowAllConnections;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_LIST)
    {
        return MidiNetworkUdpHostConnectionPolicy::AllowFromIpList;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_RANGE)
    {
        return MidiNetworkUdpHostConnectionPolicy::AllowFromIpRange;
    }
    else
    {
        // default is any
        return MidiNetworkUdpHostConnectionPolicy::AllowAllConnections;
    }
}



_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::ValidateHostDefinition(
    MidiNetworkUdpHostDefinition& definition,
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

    if (definition.ConnectionPolicy == MidiNetworkUdpHostConnectionPolicy::AllowFromIpRange)
    {
        // validate that there are exactly two entries

        if (definition.IpAddresses.size() != 2)
        {
            errorMessage = L"Connection policy IP address range needs exactly two valid entries to define the range.";
            return E_INVALIDARG;
        }
    }
    else if (definition.ConnectionPolicy == MidiNetworkUdpHostConnectionPolicy::AllowFromIpList)
    {
        // validate that there is at least one entry

        if (definition.IpAddresses.size() < 1)
        {
            errorMessage = L"Connection policy IP address list needs at least one valid entry.";
            return E_INVALIDARG;
        }

    }


    // validate user authentication



    return S_OK;

}


_Use_decl_annotations_
HRESULT
CMidi2NetworkMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    BOOL isFromConfigurationFile,
    BSTR* response
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

    UNREFERENCED_PARAMETER(isFromConfigurationFile);

    // if we're passed a null or empty json, we just quietly exit
    if (configurationJsonSection == nullptr) return S_OK;

    // default to failure


    // Json Format for network UDP midi
    //
    //{
    //    "{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}":
    //    {
    //        "_comment": "Network UDP MIDI 2.0",
    //            "create" :
    //            [
    //                {
    //                    "host":
    //                    {
    //                        "entryIdentifier": "{62153fff-a0d0-4c0a-98ad-09010123d10a}",
    //                        "name" : "my_endpoint",
    //                        "hostInstanceName" : "windowsproto", 
    //                        "productInstanceId" : "8675309",
    //                        "networkInterface" : "all",
    //                        "networkProtocol" : "udp",
    //                        "networkPort" : "auto",
    //                        "umpOnly" : true,
    //                        "enabled" : true,
    //                        "connectionRulesIpv4" :
    //                        {
    //                          "allow" : "list",
    //                          "addresses" :
    //                          [
    //                            "192.168.1.1",
    //                            "192.168.1.10"
    //                          ]
    //                        }
    //                        "authentication" : "none" 
    //                    }
    //                },
    //                "client":
    //                {
    //                   TODO: Clients for connect / reconnect
    //                }
    //            ],
    //            "remove":
    //            [
    //
    //            ]
    //
    //
    //    }
    //}


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

        internal::JsonStringifyObjectToOutParam(responseObject, &response);

        RETURN_IF_FAILED(E_INVALIDARG);
    }

    auto createArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);

    if (createArray == nullptr || createArray.Size() == 0)
    {
        // nothing in the array. Maybe we're looking at update or delete 

        // TODO: Set the response to something meaningful here

        internal::JsonStringifyObjectToOutParam(responseObject, &response);

        // once we enable update/delete we need to move this
        return S_OK;
    }


    // iterate through all the work we need to do. Just 
    // "create" instructions, in this case.
    for (uint32_t i = 0; i < createArray.Size(); i++)
    {
        auto jsonEntry = (createArray.GetObjectAt(i));

        if (jsonEntry)
        {
            // are we defining a host?
            if (auto hostEntry = jsonEntry.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_KEY, nullptr); hostEntry != nullptr)
            {
                MidiNetworkUdpHostDefinition definition;
                winrt::hstring validationErrorMessage{};

                // currently, UDP is the only allowed protocol

                auto protocol = internal::ToLowerTrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

                if (protocol != MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
                {
                    validationErrorMessage = L"Invalid network protocol '" + protocol + L"' specified.";

                }

                definition.EntryIdentifier = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_ENTRY_IDENTIFIER_KEY, L""));

                definition.UmpEndpointName = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L""));
                definition.ProductInstanceId = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY, L""));

                definition.Authentication = MidiNetworkUdpHostAuthenticationFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_KEY, L""));
                definition.ConnectionPolicy = MidiNetworkUdpHostConnectionPolicyFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_KEY, L""));

                // read the list of ip info
                if (definition.ConnectionPolicy != MidiNetworkUdpHostConnectionPolicy::AllowAllConnections)
                {
                    auto addressArray = hostEntry.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_IPV4_ADDRESSES_KEY);

                    for (uint32_t j = 0; j < addressArray.Size(); j++)
                    {
                        auto addressEntry = addressArray.GetStringAt(j);

                        HostName address(addressEntry);

                        definition.IpAddresses.push_back(address);
                    }
                }

                // read authentication information
                if (definition.Authentication != MidiNetworkUdpHostAuthentication::NoAuthentication)
                {

                    if (definition.Authentication == MidiNetworkUdpHostAuthentication::PasswordAuthentication)
                    {
                        // TODO: Read the password vault key
                    }
                    else if (definition.Authentication == MidiNetworkUdpHostAuthentication::UserAuthentication)
                    {
                        // TODO: Read username/password vault key
                    }
                    else
                    {
                        // no authentication provided
                    }

                }

                // generate host name and other info

                auto serviceInstanceNamePrefix = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY, L""));

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

                definition.ServiceInstanceName = serviceInstanceNamePrefix;

                // TODO: See if the serviceInstanceName is already in use. If so, add a disambiguation number




                // validate the entry

                if (SUCCEEDED(ValidateHostDefinition(definition, validationErrorMessage)))
                {
                    // create the host

                    auto host = std::make_shared<MidiNetworkHost>();

                    RETURN_HR_IF_NULL(E_POINTER, host);
                    RETURN_IF_FAILED(host->Initialize(definition));

                    host->Start();
                }
                else
                {
                    // invalid entry
                }



            }
            else if (auto clientEntry = jsonEntry.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CLIENT_KEY, nullptr); clientEntry != nullptr)
            {
                // TODO: Add to the client reconnect table
            }



        //    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

        //    // create the device-side endpoint. This is a critical step
        //    RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->CreateDeviceSideEndpoint(deviceEntry));

        //    // mark success
        //    responseObject.SetNamedValue(
        //        MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
        //        jsonTrue);

        //    // TODO: This should have the association Id or something in it for the client to make sense of it
        //    auto singleResponse = internal::JsonCreateSingleWStringPropertyObject(
        //        MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY,
        //        deviceEntry.CreatedDeviceEndpointId);

        //    createdDevicesResponseArray.Append(singleResponse);
        }
    }

    //responseObject.SetNamedValue(
    //    MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY,
    //    createdDevicesResponseArray);

    // TODO: Process all "update" and "remove" instructions


    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(responseObject.Stringify().c_str())
    );

    // return the json with the information the client will need
    internal::JsonStringifyObjectToOutParam(responseObject, &response);

    return S_OK;
}


HRESULT
CMidi2NetworkMidiConfigurationManager::Cleanup()
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

