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
    UNREFERENCED_PARAMETER(abstractionGuid);
    UNREFERENCED_PARAMETER(MidiServiceConfigurationManagerInterface);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    return S_OK;
}


// internal function called by the endpoint manager
_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::ApplyConfigFileUpdatesForEndpoint(std::wstring endpointSearchKeysJson)
{
    UNREFERENCED_PARAMETER(endpointSearchKeysJson);


    //auto id = internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

    //auto match = std::find_if(
    //    m_CachedConfigurationUpdates.begin(),
    //    m_CachedConfigurationUpdates.end(),
    //    [&id](const std::unique_ptr<ConfigUpdateForEndpoint>& x) { return x->DeviceInterfaceId == id; });

    //if (match != m_CachedConfigurationUpdates.end())
    //{
    //    return match->get();
    //}
    //else
    //{
    //    return nullptr;
    //}


    return S_OK;

}


_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection, 
    BOOL IsFromConfigurationFile, 
    BSTR* Response)
{
    // default to failure
    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(ConfigurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse Configuration JSON", "message"),
                TraceLoggingWideString(ConfigurationJsonSection, "json")
            );

            internal::JsonStringifyObjectToOutParam(responseObject, &Response);

            return E_FAIL;
        }


        // we only do updates if they are from the config file. Nothing temporary

        if (IsFromConfigurationFile)
        {
            auto updateArray = jsonObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

            if (updateArray != nullptr && updateArray.Size() > 0)
            {
                std::vector<DEVPROPERTY> endpointProperties;

                auto iter = updateArray.First();

                while (iter.HasCurrent())
                {
                    auto o = iter.Current().GetObject();

                    // get the device id, Right now, the SWD is the only way to id the item. We're changing that
                    auto swdId = o.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_SEARCH_PROPERTY_KEY_SWD, L"");

                    // TODO: Validate that the device is valid

                    // user-supplied name
                    if (o.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY))
                    {
                        auto val = o.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY);

                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_STRING, static_cast<ULONG>((val.size() + 1) * sizeof(WCHAR)), (PVOID)val.c_str() });
                    }
                    else
                    {
                        // delete any existing property value, because it is no longer in the config
                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_EMPTY, 0, nullptr });

                    }

                    // user-supplied description
                    if (o.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY))
                    {
                        auto val = o.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY);

                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_STRING, static_cast<ULONG>((val.size() + 1) * sizeof(WCHAR)), (PVOID)val.c_str() });
                    }
                    else
                    {
                        // delete any existing property value, because it is no longer in the config
                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_EMPTY, 0, nullptr });
                    }

                    // user-supplied small image
                    if (o.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY))
                    {
                        auto val = o.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY);

                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_STRING, static_cast<ULONG>((val.size() + 1) * sizeof(WCHAR)), (PVOID)val.c_str() });
                    }
                    else
                    {
                        // delete any existing property value, because it is no longer in the config
                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_EMPTY, 0, nullptr });
                    }

                    // user-supplied large image
                    if (o.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY))
                    {
                        auto val = o.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY);

                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_STRING, static_cast<ULONG>((val.size() + 1) * sizeof(WCHAR)), (PVOID)val.c_str() });
                    }
                    else
                    {
                        // delete any existing property value, because it is no longer in the config
                        endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                                DEVPROP_TYPE_EMPTY, 0, nullptr });
                    }

                    // set all the properties
                    if (endpointProperties.size() > 0)
                    {
                        LOG_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(
                            swdId.c_str(),
                            (ULONG)endpointProperties.size(),
                            (PVOID)endpointProperties.data()
                        ));
                    }

                    iter.MoveNext();
                }

                responseObject.SetNamedValue(
                    MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY,
                    json::JsonValue::CreateBooleanValue(true));

            }
        }
    }
    catch (const std::exception& e)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"std exception processing json", "message"),
            TraceLoggingString(e.what(), "exception"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Other exception processing json", "message"),
            TraceLoggingWideString(ConfigurationJsonSection, "json")
        );
    }

    internal::JsonStringifyObjectToOutParam(responseObject, &Response);

    return S_OK;

    //try
    //{
    //    // if we have no configuration, that's ok
    //    if (m_jsonObject == nullptr) return S_OK;


    //    std::vector<DEVPROPERTY> endpointProperties;

    //    // for now, we only support lookup by the deviceInterfaceId, so this code is easier

    //    winrt::hstring endpointSettingsKey = winrt::to_hstring(MIDI_CONFIG_JSON_ENDPOINT_IDENTIFIER_SWD) + deviceInterfaceId;

    //    //OutputDebugString(L" Key: ");
    //    //OutputDebugString(endpointSettingsKey.c_str());
    //    //OutputDebugString(L"\n");


    //    if (m_jsonObject.HasKey(endpointSettingsKey))
    //    {
    //        json::JsonObject endpointSettings = m_jsonObject.GetNamedObject(endpointSettingsKey);

    //        // Get the user-specified endpoint name
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY))
    //        {
    //            auto name = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY);

    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_STRING, static_cast<ULONG>((name.size() + 1) * sizeof(WCHAR)), (PVOID)name.c_str() });
    //        }
    //        else
    //        {
    //            // delete any existing property value, because it is no longer in the config
    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_EMPTY, 0, nullptr });
    //        }


    //        // Get the user-specified endpoint description
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY))
    //        {
    //            auto description = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY);

    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_STRING, static_cast<ULONG>((description.size() + 1) * sizeof(WCHAR)), (PVOID)description.c_str() });
    //        }
    //        else
    //        {
    //            // delete any existing property value, because it is no longer in the config
    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_EMPTY, 0, nullptr });
    //        }

    //        // Get the user-specified multiclient override
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY))
    //        {
    //            auto forceSingleClient = endpointSettings.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY);

    //            if (forceSingleClient)
    //            {
    //                DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    //                endpointProperties.push_back({ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
    //                        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), (PVOID)&devPropFalse });
    //            }
    //        }
    //        else
    //        {
    //            // this property was an override, so it should have been set elsewhere earlier
    //        }

    //        // apply supported property changes.
    //        if (endpointProperties.size() > 0)
    //        {
    //            return m_MidiDeviceManager->UpdateEndpointProperties(
    //                (LPCWSTR)deviceInterfaceId.c_str(),
    //                (ULONG)endpointProperties.size(),
    //                (PVOID)endpointProperties.data());
    //        }
    //    }

    //    return S_OK;
    //}
    //catch (...)
    //{
    //    return E_FAIL;
    //}
}


HRESULT
CMidi2KSMidiConfigurationManager::Cleanup()
{
    return S_OK;
}
