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

    if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE)
    {
        return MidiNetworkHostAuthentication::NoAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD)
    {
        return MidiNetworkHostAuthentication::PasswordAuthentication;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER)
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

    if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY)
    {
        return MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_LIST)
    {
        return MidiNetworkHostConnectionPolicy::PolicyAllowFromIpList;
    }
    else if (value == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_RANGE)
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

    auto createArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, nullptr);
    auto updateArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);
    auto removeArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, nullptr);

    if (createArray != nullptr && createArray.Size() > 0)
    {
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
                    MidiNetworkHostDefinition definition;
                    winrt::hstring validationErrorMessage{ };

                    // currently, UDP is the only allowed protocol

                    auto protocol = internal::ToLowerTrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_KEY, MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP));

                    if (protocol != MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP)
                    {
                        validationErrorMessage = L"Invalid network protocol '" + protocol + L"' specified.";
                    }

                    definition.Advertise = hostEntry.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_MDNS_ADVERTISE_KEY, true);

                    definition.Port = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PORT_KEY, MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO));

                    definition.EntryIdentifier = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_ENTRY_IDENTIFIER_KEY, L""));

                    definition.UmpEndpointName = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, L""));
                    definition.ProductInstanceId = internal::TrimmedHStringCopy(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY, L""));

                    definition.Authentication = MidiNetworkHostAuthenticationFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_KEY, MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE));
                    definition.ConnectionPolicy = MidiNetworkHostConnectionPolicyFromJsonString(hostEntry.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_KEY, MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY));

                    // read the list of ip info
                    //if (definition.ConnectionPolicy != MidiNetworkHostConnectionPolicy::PolicyAllowAllConnections)
                    //{
                    //    auto addressArray = hostEntry.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_IPV4_ADDRESSES_KEY);

                    //    for (uint32_t j = 0; j < addressArray.Size(); j++)
                    //    {
                    //        auto addressEntry = addressArray.GetStringAt(j);

                    //        HostName address(addressEntry);

                    //        definition.IpAddresses.push_back(address);
                    //    }
                    //}

                    // read authentication information
                    if (definition.Authentication != MidiNetworkHostAuthentication::NoAuthentication)
                    {

                        if (definition.Authentication == MidiNetworkHostAuthentication::PasswordAuthentication)
                        {
                            // TODO: Read the password vault key
                        }
                        else if (definition.Authentication == MidiNetworkHostAuthentication::UserAuthentication)
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
                    

                    // TODO: See if the serviceInstanceName is already in use. If so, add a disambiguation number. Keep trying until unused one is found

                    //definition.HostName = definition.ServiceInstanceName + L"._midi2._udp.local";

                    // TODO: User should be able to specify the adapter, host name, etc.

                    auto hostNames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();

                    for (auto const& host : hostNames)
                    {
                        if ((host.Type() == HostNameType::DomainName) &&
                            (host.RawName().ends_with(L".local")))
                        {
                            definition.HostName = host.RawName();
                            break;
                        }
                    }                  
                    

                    if (definition.Port == MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO || 
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


                    // validate the entry

                    if (SUCCEEDED(ValidateHostDefinition(definition, validationErrorMessage)))
                    {
                        TraceLoggingWrite(
                            MidiNetworkMidiTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Host definition validated. Creating host", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                        );

                        // create the host

                        auto host = std::make_shared<MidiNetworkHost>();

                        RETURN_HR_IF_NULL(E_POINTER, host);
                        RETURN_IF_FAILED(host->Initialize(definition));

                        // add to our collection of hosts
                        TransportState::Current().AddHost(host);

                        // we only start if this comes in after the endpoint manager has been spun up
                        if (TransportState::Current().GetEndpointManager() != nullptr && 
                            TransportState::Current().GetEndpointManager()->IsInitialized())
                        {
                            RETURN_IF_FAILED(host->Start());
                        }
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

            }
        }
    }

    if (updateArray != nullptr && updateArray.Size() > 0)
    {
        // TODO: Implement update
    }

    if (removeArray != nullptr && removeArray.Size() > 0)
    {
        // TODO: Implement remove
    }


    //responseObject.SetNamedValue(
    //    MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY,
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

