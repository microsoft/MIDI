// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "json_transport_command_helper.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManagerInterface
)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(transportId);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));

    RETURN_IF_FAILED(midiServiceConfigurationManagerInterface->QueryInterface(__uuidof(IMidiServiceConfigurationManager), (void**)&m_midiServiceConfigurationManagerInterface));


    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::ProcessCommand(
    json::JsonObject const& transportObject,
    json::JsonObject& responseObject)
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

        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_ENDPOINT, true);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_CUSTOMIZE_PORTS, true);

        // revisit these once the functions are added in
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RESTART_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_DISCONNECT_ENDPOINT, false);
        capabilities.emplace(MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_RECONNECT_ENDPOINT, false);

        internal::SetConfigurationResponseObjectSuccess(responseObject);
        internal::SetConfigurationCommandResponseQueryCapabilities(responseObject, capabilities);

    }
    else
    {
        internal::SetConfigurationResponseObjectFail(responseObject, L"Unrecognized command.");
    }

    // we return S_OK no matter what, so the response object will be parsed
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::ProcessCustomProperties(
    winrt::hstring resolvedEndpointDeviceId,
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> matchCriteria,
    json::JsonObject updateObject,
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties>& customProperties,
    std::vector<DEVPROPERTY>& endpointDevProperties,
    std::shared_ptr<WindowsMidiServicesNamingLib::MidiEndpointNameTable>& nameTable,
    json::JsonObject& responseObject)
{
    UNREFERENCED_PARAMETER(responseObject);

    // Check for common custom properties (Name, Description, Image, etc.)
    if (updateObject.HasKey(WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey))
    {
        auto customPropsJson = updateObject.GetNamedObject(WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey);

        customProperties = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJson(customPropsJson);

        if (customProperties != nullptr)
        {
            // cache this set of properties in case of surprise removal or the device is added later
            m_customPropertiesCache->Add(matchCriteria, customProperties);

            if (customProperties->WriteAllProperties(endpointDevProperties))
            {
                if (customProperties->Midi1Destinations.size() > 0 ||
                    customProperties->Midi1Sources.size() > 0)
                {
                    if (!resolvedEndpointDeviceId.empty())
                    {
                        // get the current name table
                        nameTable = WindowsMidiServicesNamingLib::MidiEndpointNameTable::FromEndpointDeviceId(resolvedEndpointDeviceId);

                        // apply name updates
                        for (auto const& source : customProperties->Midi1Sources)
                        {
                            nameTable->UpdateSourceEntryCustomName(source.second.GroupIndex, source.second.Name);
                        }

                        for (auto const& dest : customProperties->Midi1Destinations)
                        {
                            nameTable->UpdateDestinationEntryCustomName(dest.second.GroupIndex, dest.second.Name);
                        }

                        // write out the dev props. This does require that the name table is kept around
                        // until the props are written, which is why it was passed in as a param
                        nameTable->WriteProperties(endpointDevProperties);
                    }
                }
            }
            else
            {
                // failed to write custom properties
                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Failed writing custom user properties", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );
            }
        }
        else
        {
            // failed to read custom properties
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed reading custom user properties", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }
    }


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection, 
    LPWSTR* response)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, configurationJsonSection);
    RETURN_HR_IF_NULL(E_INVALIDARG, response);
    RETURN_HR_IF(E_INVALIDARG, wcslen(configurationJsonSection) == 0);

    if (m_midiDeviceManager == nullptr)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Device Manager is nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(configurationJsonSection, "json")
        );

        return E_FAIL;
    }


    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(configurationJsonSection, "json")
            );

            //internal::JsonStringifyObjectToOutParam(responseObject, &response);

            return E_INVALIDARG;
        }

        // command. If there's a command in the payload, we ignore anything else
        if (internal::MidiTransportCommandHelper::TransportObjectContainsCommand(jsonObject))
        {
            auto hr = ProcessCommand(jsonObject, responseObject);

            internal::JsonStringifyObjectToOutParam(responseObject, response);

            return hr;
        }

        // get all the updates we need to process
        auto updateArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

        if (updateArray != nullptr && updateArray.Size() > 0)
        {
            for (auto const& updateVal : updateArray)
            {
                auto updateObject = updateVal.GetObject();
                if (updateObject == nullptr)
                {
                    // unexpected non-object in the array. Could be a comment or something. Ignore and move on.
                    continue;
                }

                auto matchObject = updateObject.GetNamedObject(WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey, nullptr);
                if (matchObject == nullptr)
                {
                    // no match object found, so no away to associate this update to an endpoint. Move on.
                    continue;
                }

                std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties{ nullptr };
                std::shared_ptr<WindowsMidiServicesNamingLib::MidiEndpointNameTable> nameTable{ nullptr };
                std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria> matchCriteria = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(matchObject);
                std::vector<DEVPROPERTY> endpointDevProperties{};

                // Resolve the EndpointDeviceId in case we matched on something else
                winrt::hstring matchingEndpointDeviceId{};
                auto em = TransportState::Current().GetEndpointManager();

                if (em != nullptr)
                {
                    matchingEndpointDeviceId = em->FindMatchingInstantiatedEndpoint(*matchCriteria);
                }

                // process all the custom props like Name, Description, Image, etc.
                LOG_IF_FAILED(ProcessCustomProperties(
                    matchingEndpointDeviceId,
                    matchCriteria,
                    updateObject,
                    customProperties,
                    endpointDevProperties,
                    nameTable,
                    responseObject));

                if (endpointDevProperties.size() == 0)
                {
                    // no properties to update, so move on
                    continue;
                }

                if (!matchingEndpointDeviceId.empty())
                {
                    // The device manager checks to see if the endpoint exists
                    // and will return a E_NOTFOUND if it doesn't.

                    auto updatePropsHR = m_midiDeviceManager->UpdateEndpointProperties(
                        matchingEndpointDeviceId.c_str(),
                        (ULONG)endpointDevProperties.size(),
                        endpointDevProperties.data()
                    );

                    if (SUCCEEDED(updatePropsHR))
                    {
                        TraceLoggingWrite(
                            MidiKSTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Properties updated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(matchCriteria->EndpointDeviceId.c_str(), "swd")
                        );
                    }
                    else
                    {
                        // TODO: We need to update the MIDI 1 endpoint naming table as well, and then recreate the MIDI 1 ports

                        if (updatePropsHR != E_NOTFOUND)
                        {
                            TraceLoggingWrite(
                                MidiKSTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Error updating device properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(updatePropsHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                TraceLoggingWideString(matchCriteria->EndpointDeviceId.c_str(), "swd")
                            );

                            RETURN_IF_FAILED(E_FAIL);
                        }
                    }
                }
            }

            internal::SetConfigurationResponseObjectSuccess(responseObject);
        }
    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"std exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingString(e.what(), "exception"),
            TraceLoggingWideString(configurationJsonSection, "json")
        );

        return E_FAIL;
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Other exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(configurationJsonSection, "json")
        );

        return E_FAIL;
    }

    internal::JsonStringifyObjectToOutParam(responseObject, response);

    return S_OK;
}


HRESULT
CMidi2KSMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    TransportState::Current().Shutdown();

    m_midiServiceConfigurationManagerInterface.reset();
    m_midiDeviceManager.reset();

    return S_OK;
}
