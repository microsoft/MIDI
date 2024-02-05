// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include "atlbase.h"  // for CComBSTR


namespace json = ::winrt::Windows::Data::Json;

// structural

#define MIDI_CONFIG_JSON_HEADER_OBJECT L"header"
#define MIDI_CONFIG_JSON_HEADER_PRODUCT_KEY L"product"
#define MIDI_CONFIG_JSON_HEADER_FILE_VERSION_KEY L"fileVersion"



#define MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT L"endpointTransportPluginSettings"
#define MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT L"endpointProcessingPluginSettings"

// common properties

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY                          L"name"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY                   L"description"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY            L"userSuppliedName"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY     L"userSuppliedDescription"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY     L"userSuppliedSmallImage"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY     L"userSuppliedLargeImage"


// Virtual MIDI (here because also needed by the client API)

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY              L"createVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_UPDATE_ARRAY_KEY              L"updateVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_REMOVE_ARRAY_KEY              L"removeVirtualDevices"

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY    L"associationIdentifier"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_PROPERTY_KEY         L"uniqueIdentifier"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_MAX_LEN  32


#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY          L"success"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY     L"createdDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY       L"id"


// Session tracker. These are used in the service and in the client API

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY             L"sessions"

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY                L"id"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY              L"name"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY                L"clientProcessId"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY              L"processName"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY              L"startTime"


#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY          L"connections"

#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY           L"earliestStartTime"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY          L"instanceCount"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY    L"endpointId"



namespace Windows::Devices::Midi2::Internal
{
    inline bool JsonObjectFromBSTR(_In_ BSTR* bstr, _Out_ json::JsonObject &obj)
    {
        if (bstr == nullptr) return false;

        try
        {
            ATL::CComBSTR ccbstr(*bstr);
            if (ccbstr.Length() == 0) return false;

            winrt::hstring hstr(ccbstr);

            if (json::JsonObject::TryParse(hstr, obj))
            {
                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }



    inline void JsonStringifyObjectToOutParam(_In_ json::JsonObject obj, _Out_ BSTR** outParam)
    {
        ATL::CComBSTR responseString = obj.Stringify().c_str();
        responseString.CopyTo(*outParam);
    }


    inline json::JsonObject JsonCreateSingleWStringPropertyObject(_In_ std::wstring key, _In_ std::wstring value)
    {
        json::JsonObject obj;

        obj.SetNamedValue(key, json::JsonValue::CreateStringValue(value.c_str()));

        return obj;
    }



    inline json::JsonArray JsonGetArrayProperty(_In_ json::JsonObject parent, _In_ std::wstring key) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedArray(key);

                    return result;
                }
                catch (...)
                {
                    // we'll fall through and return an empty array
                }

            }
        }

        // return an empty array in case of error
        return json::JsonArray{};
    }

    inline bool JsonSetArrayProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ json::JsonArray value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                parent.SetNamedValue(key, value);

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }


    inline std::wstring JsonGetWStringProperty(_In_ json::JsonObject parent, _In_ std::wstring key, std::wstring defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedString(key);

                    return result.c_str();
                }
                catch (...)
                {
                    // we'll fall through and return the default
                }

            }
        }

        // return default in case of error
        return defaultValue;
    }

    inline bool JsonSetWStringProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ std::wstring value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                winrt::hstring stringValue = winrt::to_hstring(value.c_str());

                parent.SetNamedValue(key, json::JsonValue::CreateStringValue(stringValue));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }



    inline GUID JsonGetGuidProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ GUID defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedString(key);

                    return StringToGuid(result.c_str());
                }
                catch (...)
                {
                    // we'll fall through and return the default
                }
            }
        }

        // return default in case of error
        return defaultValue;
    }

    inline bool JsonSetGuidProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ GUID value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                winrt::hstring stringValue = winrt::to_hstring(GuidToString(value).c_str());

                parent.SetNamedValue(key, json::JsonValue::CreateStringValue(stringValue));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }


    inline bool JsonGetBoolProperty(_In_ json::JsonObject parent, _In_ std::wstring key, bool defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedBoolean(key);

                    return result;
                }
                catch (...)
                {
                    // we'll fall through and return the default
                }

            }
        }

        // return default in case of error
        return defaultValue;
    }

    inline bool JsonSetBoolProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ bool value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                parent.SetNamedValue(key, json::JsonValue::CreateBooleanValue(value));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }



    inline double JsonGetDoubleProperty(_In_ json::JsonObject parent, _In_ std::wstring key, double defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedNumber(key);

                    return result;
                }
                catch (...)
                {
                    // we'll fall through and return the default
                }

            }
        }

        // return default in case of error
        return defaultValue;
    }

    inline bool JsonSetDoubleProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ double value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                parent.SetNamedValue(key, json::JsonValue::CreateNumberValue(value));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }






    inline json::JsonObject JsonGetObjectProperty(_In_ json::JsonObject parent, _In_ std::wstring key, json::JsonObject defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            if (parent.HasKey(key))
            {
                try
                {
                    auto result = parent.GetNamedObject(key);

                    return result;
                }
                catch (...)
                {
                    // we'll fall through and return the default
                }

            }
        }

        // return default in case of error
        return defaultValue;
    }

    inline bool JsonSetObjectProperty(_In_ json::JsonObject parent, _In_ std::wstring key, _In_ json::JsonObject value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                parent.SetNamedValue(key, value);

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }











}

