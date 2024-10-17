// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "atlbase.h"  // for CComBSTR


namespace json = ::winrt::Windows::Data::Json;


namespace WindowsMidiServicesInternal
{
    inline json::JsonObject BuildConfigurationResponseObject(_In_ bool const success)
    {
        // the root object just has a wrapper with a success or fail property. Additional 
        // properties depend on the specific use case and so are added in those cases.

        json::JsonObject response;

        json::JsonValue successValue = json::JsonValue::CreateBooleanValue(success);

        response.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, successValue);

        return response;        
    }

    inline void SetConfigurationResponseObjectFail(_In_ json::JsonObject& object, _In_ std::wstring message)
    {
        auto messageVal = json::JsonValue::CreateStringValue(message.c_str());
        object.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY, messageVal);

        auto successVal = json::JsonValue::CreateBooleanValue(false);
        object.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, messageVal);
    }

    _Success_(return == true)
    inline bool JsonObjectFromBSTR(_In_ BSTR* const jsonBString, _Out_ json::JsonObject &obj) noexcept
    {
        if (jsonBString != nullptr)
        {
            try
            {
                ATL::CComBSTR ccbstr(*jsonBString);
                if (ccbstr.Length() > 0)
                {
                    winrt::hstring jsonHString(ccbstr);

                    // SAL doesn't understand that TryParse returns true on success
                    // and so it complains with warning C6101 that obj is uninitialized
                    // on a success path in this function.
                    if (json::JsonObject::TryParse(jsonHString, obj))
                    {
                        return true;
                    }
                }
            }
            catch (...)
            {
            }
        }

        return false;
    }

    _Success_(return == true)
    inline bool JsonStringifyObjectToOutParam(
        _In_ json::JsonObject const& obj, 
        _Out_ LPWSTR* outParam) noexcept
    {
        try
        {
            wil::unique_cotaskmem_string tempString = wil::make_cotaskmem_string_nothrow(obj.Stringify().c_str());

            if (tempString.get() != nullptr)
            {
                *outParam = (LPWSTR)tempString.release();
                return true;
            }

            return false;
        }
        catch (...)
        {

        }

        return false;
    }

    inline json::JsonObject JsonCreateSingleWStringPropertyObject(_In_ std::wstring const& key, _In_ std::wstring const& value) noexcept
    {
        json::JsonObject obj;

        obj.SetNamedValue(key, json::JsonValue::CreateStringValue(value.c_str()));

        return obj;
    }

    // this handles accuracy of seconds and no finer
    inline std::chrono::time_point<std::chrono::system_clock> 
    JsonGetDateTimeProperty(
        _In_ json::JsonObject const& parent, 
        _In_ std::wstring const& key, 
        _In_ std::chrono::time_point<std::chrono::system_clock> const defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedNumber(hkey);
                    auto seconds = std::chrono::seconds((long)result);

                    std::chrono::time_point<std::chrono::system_clock> tp;
                    tp += seconds;

                    return tp;
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

    // this handles accuracy of seconds and no finer
    inline bool JsonSetDateTimeProperty(
        _In_ json::JsonObject const& parent, 
        _In_ std::wstring const& key, 
        _In_ std::chrono::time_point<std::chrono::system_clock> const value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                auto number = std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count();

                parent.SetNamedValue(key, json::JsonValue::CreateNumberValue((double)number));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }

    inline GUID JsonGetGuidProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ GUID const& defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedString(hkey);

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

    inline bool JsonSetGuidProperty(_In_ json::JsonObject const& parent, _In_ std::wstring const& key, _In_ GUID const& value) noexcept
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


}

#endif