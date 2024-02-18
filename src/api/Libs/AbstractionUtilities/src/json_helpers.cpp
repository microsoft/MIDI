// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

namespace Windows::Devices::Midi2::Internal
{
    _Use_decl_annotations_
    void SetConfigurationResponseObjectFail(
        json::JsonObject& object,
        std::wstring message)
    {
        auto messageVal = json::JsonValue::CreateStringValue(message.c_str());
        object.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY, messageVal);

        auto successVal = json::JsonValue::CreateBooleanValue(false);
        object.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, messageVal);
    }



    _Use_decl_annotations_
    json::JsonObject BuildConfigurationResponseObject(_In_ bool const success)
    {
        // the root object just has a wrapper with a success or fail property. Additional 
        // properties depend on the specific use case and so are added in those cases.

        json::JsonObject response;

        json::JsonValue successValue = json::JsonValue::CreateBooleanValue(success);

        response.SetNamedValue(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, successValue);

        return response;
    }



    _Use_decl_annotations_
    bool JsonObjectFromBSTR(BSTR* const bstr, json::JsonObject& obj) noexcept
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

        obj = nullptr;
        return false;
    }



    _Use_decl_annotations_
    bool JsonStringifyObjectToOutParam(json::JsonObject const& obj, BSTR** outParam) noexcept
    {
        try
        {
            ATL::CComBSTR responseString = obj.Stringify().c_str();

            auto hr = responseString.CopyTo(*outParam);

            if (SUCCEEDED(hr))
            {
                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }

    _Use_decl_annotations_
    json::JsonObject JsonCreateSingleWStringPropertyObject(std::wstring const& key, std::wstring const& value) noexcept
    {
        json::JsonObject obj;

        obj.SetNamedValue(key, json::JsonValue::CreateStringValue(value.c_str()));

        return obj;
    }


    _Use_decl_annotations_
    json::JsonArray JsonGetArrayProperty(json::JsonObject const& parent, std::wstring const& key) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedArray(hkey);

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

    _Use_decl_annotations_
    bool JsonSetArrayProperty(json::JsonObject const& parent, std::wstring const& key, json::JsonArray const& value) noexcept
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

    _Use_decl_annotations_
    std::wstring JsonGetWStringProperty(json::JsonObject const& parent, std::wstring const& key, std::wstring const& defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedString(hkey);

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

    _Use_decl_annotations_
    bool JsonSetWStringProperty(json::JsonObject const& parent, std::wstring const& key, std::wstring const& value) noexcept
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

    // this handles accuracy of seconds and no finer
    _Use_decl_annotations_
    std::chrono::time_point<std::chrono::system_clock> 
    JsonGetDateTimeProperty(
        json::JsonObject const& parent, 
        std::wstring const& key, 
        std::chrono::time_point<std::chrono::system_clock> const defaultValue) noexcept
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
    _Use_decl_annotations_
    bool JsonSetDateTimeProperty(
        json::JsonObject const& parent, 
        std::wstring const& key, 
        std::chrono::time_point<std::chrono::system_clock> const value) noexcept
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

    _Use_decl_annotations_
    GUID JsonGetGuidProperty(json::JsonObject const& parent, std::wstring const& key, GUID const& defaultValue) noexcept
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

    _Use_decl_annotations_
    bool JsonSetGuidProperty(json::JsonObject const& parent, std::wstring const& key, GUID const& value) noexcept
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

    _Use_decl_annotations_
    bool JsonGetBoolProperty(json::JsonObject const& parent, std::wstring const& key, bool const defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedBoolean(hkey);

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

    _Use_decl_annotations_
    bool JsonSetBoolProperty(json::JsonObject const& parent, std::wstring const& key, bool const value) noexcept
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

    _Use_decl_annotations_
    double JsonGetDoubleProperty(json::JsonObject const& parent, std::wstring const& key, double const defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedNumber(hkey);

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

    _Use_decl_annotations_
    bool JsonSetDoubleProperty(json::JsonObject const& parent, std::wstring const& key, double const value) noexcept
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

    _Use_decl_annotations_
    long JsonGetLongProperty(json::JsonObject const& parent, std::wstring const& key, long const defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = (long)parent.GetNamedNumber(hkey);

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

    _Use_decl_annotations_
    bool JsonSetLongProperty(json::JsonObject const& parent, std::wstring const& key, long const value) noexcept
    {
        try
        {
            if (parent != nullptr)
            {
                parent.SetNamedValue(key, json::JsonValue::CreateNumberValue((double)value));

                return true;
            }
        }
        catch (...)
        {

        }

        return false;
    }






    _Use_decl_annotations_
    json::JsonObject JsonGetObjectProperty(json::JsonObject const& parent, std::wstring const& key, json::JsonObject const& defaultValue) noexcept
    {
        if (parent != nullptr)
        {
            auto hkey = winrt::to_hstring(key.c_str());

            if (parent.HasKey(hkey))
            {
                try
                {
                    auto result = parent.GetNamedObject(hkey);

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

    _Use_decl_annotations_
    bool JsonSetObjectProperty(json::JsonObject const& parent, std::wstring const& key, json::JsonObject const& value) noexcept
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

