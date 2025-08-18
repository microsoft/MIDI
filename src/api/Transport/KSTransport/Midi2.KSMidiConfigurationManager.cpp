// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "json_custom_property_helper.h"
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
std::wstring
CMidi2KSMidiConfigurationManager::BuildEndpointJsonSearchKeysForSWD(_In_ std::wstring endpointDeviceInterfaceId)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), "endpointDeviceInterfaceId")
    );

// [
//   {
//     "SWD" : "\\\\?\\SWD#blahblahblachbleghblurg{bigoldguid}"
//   }, 
//   ...
// ]

    json::JsonObject criteriaObject{};

    criteriaObject.SetNamedValue(
        MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, 
        json::JsonValue::CreateStringValue(internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId).c_str()));

    // criteria sets are in an array
    json::JsonArray arr{};

    arr.Append(criteriaObject);
   
    return arr.Stringify().c_str();
}



// internal function called by the endpoint manager
_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::ApplyConfigFileUpdatesForEndpoint(std::wstring endpointSearchKeysJson)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointSearchKeysJson.c_str(), "search keys")
    );

    LPWSTR endpointUpdateJsonFragment;

    auto result = m_midiServiceConfigurationManagerInterface->GetCachedEndpointUpdateEntry(
        TRANSPORT_LAYER_GUID, 
        endpointSearchKeysJson.c_str(), 
        &endpointUpdateJsonFragment);

    if (SUCCEEDED(result))
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Updating configuration", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointUpdateJsonFragment, "update json")
        );

        LPWSTR updateResponse;

        // apply updates

        auto updateResult = UpdateConfiguration(endpointUpdateJsonFragment, &updateResponse);

        if (FAILED(updateResult))
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Configuration update failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(updateResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );
        }


        return updateResult;
    }
    else
    {
        // it's ok for there to be no config update for an endpoint, so this is not a failure

        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No config file update for endpoint, or endpoint was not found.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
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
            auto commandHelper = internal::MidiTransportCommandHelper::ParseCommand(jsonObject);

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

            internal::JsonStringifyObjectToOutParam(responseObject, response);
            return S_OK;
        }





        auto updateArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

        if (updateArray != nullptr && updateArray.Size() > 0)
        {
            auto updateArrayIter = updateArray.First();

            while (updateArrayIter.HasCurrent())
            {
                auto updateObject = updateArrayIter.Current().GetObject();

                auto matchObject = updateObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY, nullptr);

                if (matchObject != nullptr)
                {
                    std::vector<DEVPROPERTY> endpointProperties{};
                    internal::MidiEndpointCustomPropertiesHelper customPropertiesHelper{}; // helper is here so props are available 

                    // get the device id, Right now, the SWD is the only way to id the item. We're changing that
                    // but when the update is sent at runtime, that's the only value that's useful anyway
                    auto swdId = internal::NormalizeEndpointInterfaceIdWStringCopy(
                        matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, L"").c_str());

                    if (!swdId.empty())
                    {
                        if (customPropertiesHelper.ReadFromJsonObject(updateObject))
                        {
                            if (!customPropertiesHelper.WriteToPropertiesVector(endpointProperties))
                            {
                                // failed to write custom properties
                                TraceLoggingWrite(
                                    MidiKSTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Failed writing custom user properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                                TraceLoggingWideString(L"Failed reading custom user properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                            );

                        }

                        // set all the properties for this SWD
                        if (endpointProperties.size() > 0)
                        {
                            TraceLoggingWrite(
                                MidiKSTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Updating properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(swdId.c_str(), "swd")
                            );

                            // The device manager checks to see if the endpoint exists
                            // and will return a E_NOTFOUND if it doesn't.

                            auto updatePropsHR = m_midiDeviceManager->UpdateEndpointProperties(
                                swdId.c_str(),
                                (ULONG)endpointProperties.size(),
                                endpointProperties.data()
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
                                    TraceLoggingWideString(swdId.c_str(), "swd")
                                );
                            }
                            else
                            {
                                if (updatePropsHR == E_NOTFOUND)
                                {
                                    // no worries. We can keep going
                                    TraceLoggingWrite(
                                        MidiKSTransportTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_WARNING,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Endpoint device doesn't exist (yet). We'll skip.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingHResult(updatePropsHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                        TraceLoggingWideString(swdId.c_str(), "swd")
                                    );
                                }
                                else
                                {
                                    TraceLoggingWrite(
                                        MidiKSTransportTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_ERROR,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Error updating device properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingHResult(updatePropsHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                        TraceLoggingWideString(swdId.c_str(), "swd")
                                    );

                                    // should we fail, or just keep going?
                                    //return E_FAIL;
                                }
                            }
                        }
                        else
                        {
                            // no changes

                            TraceLoggingWrite(
                                MidiKSTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_WARNING,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Entry did not contain any recognized updates. Skipping.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(swdId.c_str(), "swd")
                            );
                        }
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiKSTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"SWD search key was empty.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                        );

                        return E_FAIL;
                    }
                }

                updateArrayIter.MoveNext();
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
