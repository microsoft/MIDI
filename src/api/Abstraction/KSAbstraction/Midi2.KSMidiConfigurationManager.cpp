// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::Initialize(
    GUID abstractionGuid,
    IUnknown* MidiDeviceManager,
    IUnknown* MidiServiceConfigurationManagerInterface
)
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(abstractionGuid);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    RETURN_IF_FAILED(MidiServiceConfigurationManagerInterface->QueryInterface(__uuidof(IMidiServiceConfigurationManagerInterface), (void**)&m_MidiServiceConfigurationManagerInterface));


    return S_OK;
}


_Use_decl_annotations_
std::wstring
CMidi2KSMidiConfigurationManager::BuildEndpointJsonSearchKeysForSWD(_In_ std::wstring endpointDeviceInterfaceId)
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
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
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointSearchKeysJson.c_str(), "search keys")
    );

    CComBSTR endpointUpdateJsonFragment;
    endpointUpdateJsonFragment.Empty();

    auto result = m_MidiServiceConfigurationManagerInterface->GetAndPurgeConfigFileAbstractionEndpointUpdateJsonObject(
        ABSTRACTION_LAYER_GUID, 
        endpointSearchKeysJson.c_str(), 
        &endpointUpdateJsonFragment);

    if (SUCCEEDED(result))
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Updating configuration", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointUpdateJsonFragment, "update json")
        );

        CComBSTR updateResponse;
        updateResponse.Empty();

        // apply updates

        auto updateResult = UpdateConfiguration(endpointUpdateJsonFragment, true, &updateResponse);

        if (FAILED(updateResult))
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
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
            MidiKSAbstractionTelemetryProvider::Provider(),
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
    LPCWSTR ConfigurationJsonSection, 
    BOOL IsFromConfigurationFile, 
    BSTR* Response)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ConfigurationJsonSection);
    RETURN_HR_IF_NULL(E_INVALIDARG, Response);
    RETURN_HR_IF(E_INVALIDARG, wcslen(ConfigurationJsonSection) == 0);

    if (m_MidiDeviceManager == nullptr)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Device Manager is nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(ConfigurationJsonSection, "json"),
            TraceLoggingBool(IsFromConfigurationFile, "IsFromConfigurationFile")
        );

        return E_FAIL;
    }


    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(ConfigurationJsonSection, "json"),
        TraceLoggingBool(IsFromConfigurationFile, "IsFromConfigurationFile")
    );

    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(ConfigurationJsonSection, "json")
            );

            //internal::JsonStringifyObjectToOutParam(responseObject, &Response);

            return E_INVALIDARG;
        }

        // we only do updates if they are from the config file. Nothing temporary

        if (IsFromConfigurationFile)
        {
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
                        std::wstring userSuppliedEndpointName{};
                        std::wstring userSuppliedEndpointDescription{};
                        std::wstring userSuppliedSmallImagePath{};
                        std::wstring userSuppliedLargeImagePath{};

                        // get the device id, Right now, the SWD is the only way to id the item. We're changing that
                        // but when the update is sent at runtime, that's the only value that's useful anyway
                        auto swdId = internal::NormalizeEndpointInterfaceIdWStringCopy(
                            matchObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, L"").c_str());

                        if (!swdId.empty())
                        {
                            // user-supplied name
                            if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY))
                            {
                                userSuppliedEndpointName = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY).c_str());

                                if (!userSuppliedEndpointName.empty())
                                {
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_STRING, static_cast<ULONG>((userSuppliedEndpointName.length() + 1) * sizeof(WCHAR)), (PVOID)userSuppliedEndpointName.c_str() });

                                    TraceLoggingWrite(
                                        MidiKSAbstractionTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(swdId.c_str(), "swd"),
                                        TraceLoggingWideString(userSuppliedEndpointName.c_str(), "new name")
                                    );
                                }
                                else
                                {
                                    // delete any existing property value, because it is blank in the config
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_EMPTY, 0, nullptr });
                                }
                            }
                            else
                            {
                                // delete any existing property value, because it is no longer in the config
                                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });

                            }

                            // user-supplied description
                            if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY))
                            {
                                userSuppliedEndpointDescription = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY).c_str());

                                if (!userSuppliedEndpointDescription.empty())
                                {
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_STRING, static_cast<ULONG>((userSuppliedEndpointDescription.length() + 1) * sizeof(WCHAR)), (PVOID)userSuppliedEndpointDescription.c_str() });

                                    TraceLoggingWrite(
                                        MidiKSAbstractionTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(swdId.c_str(), "swd"),
                                        TraceLoggingWideString(userSuppliedEndpointDescription.c_str(), "new description")
                                    );
                                }
                                else
                                {
                                    // delete any existing property value, because it is empty
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_EMPTY, 0, nullptr });
                                }
                            }
                            else
                            {
                                // delete any existing property value, because it is no longer in the config
                                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }

                            // user-supplied small image
                            if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY))
                            {
                                userSuppliedSmallImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY).c_str());

                                if (!userSuppliedSmallImagePath.empty())
                                {
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_STRING, static_cast<ULONG>((userSuppliedSmallImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)userSuppliedSmallImagePath.c_str() });

                                    TraceLoggingWrite(
                                        MidiKSAbstractionTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(swdId.c_str(), "swd"),
                                        TraceLoggingWideString(userSuppliedSmallImagePath.c_str(), "new small image path")
                                    );
                                }
                                else
                                {
                                    // delete any existing property value, because it is blank in the config
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_EMPTY, 0, nullptr });
                                }
                            }
                            else
                            {
                                // delete any existing property value, because it is no longer in the config
                                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }

                            // user-supplied large image
                            if (updateObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY))
                            {
                                userSuppliedLargeImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY).c_str());

                                if (!userSuppliedLargeImagePath.empty())
                                {
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_STRING, static_cast<ULONG>((userSuppliedLargeImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)userSuppliedLargeImagePath.c_str() });

                                    TraceLoggingWrite(
                                        MidiKSAbstractionTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_INFO,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                        TraceLoggingPointer(this, "this"),
                                        TraceLoggingWideString(swdId.c_str(), "swd"),
                                        TraceLoggingWideString(userSuppliedLargeImagePath.c_str(), "new large image path")
                                    );
                                }
                                else
                                {
                                    // delete any existing property value, because it is blanko in the config
                                    endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                            DEVPROP_TYPE_EMPTY, 0, nullptr });
                                }
                            }
                            else
                            {
                                // delete any existing property value, because it is no longer in the config
                                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                        DEVPROP_TYPE_EMPTY, 0, nullptr });
                            }

                            // set all the properties for this SWD
                            if (endpointProperties.size() > 0)
                            {
                                TraceLoggingWrite(
                                    MidiKSAbstractionTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Updating properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(swdId.c_str(), "swd")
                                );

                                // The device manager checks to see if the endpoint exists
                                // and will return a E_NOTFOUND if it doesn't.

                                auto updatePropsHR = m_MidiDeviceManager->UpdateEndpointProperties(
                                    swdId.c_str(),
                                    (ULONG)endpointProperties.size(),
                                    (PVOID)endpointProperties.data()
                                );

                                if (SUCCEEDED(updatePropsHR))
                                {
                                    TraceLoggingWrite(
                                        MidiKSAbstractionTelemetryProvider::Provider(),
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
                                            MidiKSAbstractionTelemetryProvider::Provider(),
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
                                            MidiKSAbstractionTelemetryProvider::Provider(),
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
                                    MidiKSAbstractionTelemetryProvider::Provider(),
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
                                MidiKSAbstractionTelemetryProvider::Provider(),
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
        else
        {
            // isn't from a config file. Not really an error when it comes to logging, so we warn instead

            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Attempt to update KS device properties without them being from config file. KS device props are not runtime-temporary.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(ConfigurationJsonSection, "json")
            );

            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

            return E_INVALIDARG;
        }
    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"std exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingString(e.what(), "exception"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );

        return E_FAIL;
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Other exception processing json", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );

        return E_FAIL;
    }

    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return S_OK;
}


HRESULT
CMidi2KSMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}
