// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManagerInterface
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
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
CMidi2KSAggregateMidiConfigurationManager::BuildEndpointJsonSearchKeysForSWD(_In_ std::wstring endpointDeviceInterfaceId)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
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
CMidi2KSAggregateMidiConfigurationManager::ApplyConfigFileUpdatesForEndpoint(std::wstring endpointSearchKeysJson)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointSearchKeysJson.c_str(), "search keys")
    );

    CComBSTR endpointUpdateJsonFragment;
    endpointUpdateJsonFragment.Empty();

    auto result = m_midiServiceConfigurationManagerInterface->GetCachedEndpointUpdateEntry(
        TRANSPORT_LAYER_GUID, 
        endpointSearchKeysJson.c_str(), 
        &endpointUpdateJsonFragment);

    if (SUCCEEDED(result))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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
                MidiKSAggregateTransportTelemetryProvider::Provider(),
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
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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
CMidi2KSAggregateMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection, 
    LPWSTR* response)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, configurationJsonSection);
    RETURN_HR_IF_NULL(E_INVALIDARG, response);
    RETURN_HR_IF(E_INVALIDARG, wcslen(configurationJsonSection) == 0);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    RETURN_HR_IF_NULL(E_POINTER, m_midiDeviceManager);

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            //internal::JsonStringifyObjectToOutParam(responseObject, &response);

            RETURN_IF_FAILED(E_INVALIDARG);
        }

        // we only do updates if they are from the config file. Nothing temporary

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

                    // property updates take references, so these need to be defined at this level.
                    std::wstring customEndpointName{};
                    std::wstring customEndpointDescription{};
                    std::wstring customSmallImagePath{};
                    std::wstring customLargeImagePath{};
                    std::vector<GroupCustomPortProperty> customPortNumbers{};

                    // get the device id, Right now, the SWD is the only way to id the item. We're changing that
                    // but when the update is sent at runtime, that's the only value that's useful anyway
                    auto swdId = internal::NormalizeEndpointInterfaceIdWStringCopy(
                        matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, L"").c_str());

                    if (!swdId.empty())
                    {
                        // user-supplied name
                        if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY))
                        {
                            customEndpointName = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY).c_str());

                            if (!customEndpointName.empty())
                            {
                                endpointProperties.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_STRING, static_cast<ULONG>((customEndpointName.length() + 1) * sizeof(WCHAR)), (PVOID)customEndpointName.c_str() });

                                TraceLoggingWrite(
                                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Found user-supplied endpoint name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingWideString(customEndpointName.c_str(), "new name")
                                );
                            }
                            else
                            {
                                // delete any existing property value, because it is blank in the config
                                endpointProperties.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }
                        }
                        else
                        {
                            // delete any existing property value, because it is no longer in the config
                            endpointProperties.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_EMPTY, 0, nullptr });

                        }

                        // user-supplied description
                        if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY))
                        {
                            customEndpointDescription = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY).c_str());

                            if (!customEndpointDescription.empty())
                            {
                                endpointProperties.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_STRING, static_cast<ULONG>((customEndpointDescription.length() + 1) * sizeof(WCHAR)), (PVOID)customEndpointDescription.c_str() });

                                TraceLoggingWrite(
                                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Found user-supplied endpoint description", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingWideString(customEndpointDescription.c_str(), "new description")
                                );
                            }
                            else
                            {
                                // delete any existing property value, because it is empty
                                endpointProperties.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }
                        }
                        else
                        {
                            // delete any existing property value, because it is no longer in the config
                            endpointProperties.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_EMPTY, 0, nullptr });
                        }

                        // user-supplied small image
                        if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SMALL_IMAGE_PROPERTY))
                        {
                            customSmallImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SMALL_IMAGE_PROPERTY).c_str());

                            if (!customSmallImagePath.empty())
                            {
                                endpointProperties.push_back({ {PKEY_MIDI_CustomSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_STRING, static_cast<ULONG>((customSmallImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)customSmallImagePath.c_str() });

                                TraceLoggingWrite(
                                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Found user-supplied small image path", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingWideString(customSmallImagePath.c_str(), "new small image path")
                                );
                            }
                            else
                            {
                                // delete any existing property value, because it is blank in the config
                                endpointProperties.push_back({ {PKEY_MIDI_CustomSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }
                        }
                        else
                        {
                            // delete any existing property value, because it is no longer in the config
                            endpointProperties.push_back({ {PKEY_MIDI_CustomSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_EMPTY, 0, nullptr });
                        }

                        // user-supplied large image
                        if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_LARGE_IMAGE_PROPERTY))
                        {
                            customLargeImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_LARGE_IMAGE_PROPERTY).c_str());

                            if (!customLargeImagePath.empty())
                            {
                                endpointProperties.push_back({ {PKEY_MIDI_CustomLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_STRING, static_cast<ULONG>((customLargeImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)customLargeImagePath.c_str() });

                                TraceLoggingWrite(
                                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Found user-supplied large image path", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingWideString(customLargeImagePath.c_str(), "new large image path")
                                );
                            }
                            else
                            {
                                // delete any existing property value, because it is blanko in the config
                                endpointProperties.push_back({ {PKEY_MIDI_CustomLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }
                        }
                        else
                        {
                            // delete any existing property value, because it is no longer in the config
                            endpointProperties.push_back({ {PKEY_MIDI_CustomLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_EMPTY, 0, nullptr });
                        }

                        // retrieve user-supplied port number assignments from JSON, if present
                        if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_PORT_ASSIGNMENTS))
                        {
                            auto customPortAssignments = updateObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_PORT_ASSIGNMENTS, 0);
                            if (customPortAssignments != nullptr)
                            {
                                // TODO: parse custom port assignments out of the named object and add them to customPortNumbers vector
                            }
                        }

                        // If custom port assignments are present, add them to the endpoint properties
                        if (customPortNumbers.size() > 0)
                        {
                            endpointProperties.push_back({ {PKEY_MIDI_CustomPortAssignments, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_BINARY, (ULONG)(customPortNumbers.size() * sizeof(GroupCustomPortProperty)), (PVOID)(customPortNumbers.data()) });
                        
                            TraceLoggingWrite(
                                MidiKSAggregateTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                            );
                        }
                        else
                        {
                            // delete any existing property value, because it is no longer in the config
                            endpointProperties.push_back({ {PKEY_MIDI_CustomPortAssignments, DEVPROP_STORE_SYSTEM, nullptr},
                                    DEVPROP_TYPE_EMPTY, 0, nullptr });
                        }

                        // set all the properties for this SWD
                        if (endpointProperties.size() > 0)
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Updating properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Properties updated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                                );
                            }
                            else
                            {
                                if (updatePropsHR == E_NOTFOUND)
                                {
                                    // no worries. We can keep going
                                    TraceLoggingWrite(
                                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Endpoint device doesn't exist (yet). We'll skip.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingHResult(updatePropsHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                        TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                                    );
                                }
                                else
                                {
                                    TraceLoggingWrite(
                                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_ERROR,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(L"Error updating device properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingHResult(updatePropsHR, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                        TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                                MidiKSAggregateTransportTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_WARNING,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Entry did not contain any recognized updates. Skipping.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(swdId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                            );
                        }
                    }
                    else
                    {
                        TraceLoggingWrite(
                            MidiKSAggregateTransportTelemetryProvider::Provider(),
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

            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                json::JsonValue::CreateBooleanValue(true));
        }
    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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
CMidi2KSAggregateMidiConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
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
