// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiResourceListEntry.h"
#include "CapabilityInquiry.MidiResourceListEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        constexpr std::wstring_view FieldResource{ L"resource" };
        constexpr std::wstring_view FieldCanGet{ L"canGet" };
        constexpr std::wstring_view FieldCanSet{ L"canSet" };
        constexpr std::wstring_view FieldCanSubscribe{ L"canSubscribe" };
        constexpr std::wstring_view FieldCanPaginate{ L"canPaginate" };
        constexpr std::wstring_view FieldRequireResId{ L"requireResId" };
        constexpr std::wstring_view FieldMediaTypes{ L"mediaTypes" };
        constexpr std::wstring_view FieldEncodings{ L"encodings" };
        constexpr std::wstring_view FieldSchema{ L"schema" };

        bool ReadBoolean(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ bool const defaultValue) noexcept
        {
            try
            {
                winrt::hstring const name{ field };

                if (jsonObject.HasKey(name) &&
                    jsonObject.Lookup(name).ValueType() == json::JsonValueType::Boolean)
                {
                    return jsonObject.Lookup(name).GetBoolean();
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return defaultValue;
        }

        void ReadStringArray(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ foundation::Collections::IVector<winrt::hstring> const& destination) noexcept
        {
            try
            {
                winrt::hstring const name{ field };

                if (!jsonObject.HasKey(name) ||
                    jsonObject.Lookup(name).ValueType() != json::JsonValueType::Array)
                {
                    return;
                }

                for (auto const& element : jsonObject.Lookup(name).GetArray())
                {
                    if (element.ValueType() == json::JsonValueType::String)
                    {
                        destination.Append(element.GetString());
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }

        void WriteStringArrayIfAny(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ foundation::Collections::IVector<winrt::hstring> const& source) noexcept
        {
            try
            {
                if (source.Size() == 0) return;

                json::JsonArray array{};

                for (auto const& value : source)
                {
                    array.Append(json::JsonValue::CreateStringValue(value));
                }

                jsonObject.SetNamedValue(winrt::hstring{ field }, array);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }
    }

    json::JsonObject MidiResourceListEntry::GetJson() noexcept
    {
        try
        {
            json::JsonObject jsonObject{};

            jsonObject.SetNamedValue(winrt::hstring{ FieldResource },
                json::JsonValue::CreateStringValue(m_resource));

            // Everything else is written only when it differs from the specification's default, so
            // a device's list stays as small as the wire format expects rather than restating what
            // a reader would have assumed anyway.
            if (!m_canGet)
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldCanGet },
                    json::JsonValue::CreateBooleanValue(false));
            }

            if (m_canSet != CanSetNone())
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldCanSet },
                    json::JsonValue::CreateStringValue(m_canSet));
            }

            if (m_canSubscribe)
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldCanSubscribe },
                    json::JsonValue::CreateBooleanValue(true));
            }

            if (m_canPaginate)
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldCanPaginate },
                    json::JsonValue::CreateBooleanValue(true));
            }

            if (m_requireResourceId)
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldRequireResId },
                    json::JsonValue::CreateBooleanValue(true));
            }

            WriteStringArrayIfAny(jsonObject, FieldMediaTypes, m_mediaTypes);
            WriteStringArrayIfAny(jsonObject, FieldEncodings, m_encodings);

            if (m_schema != nullptr)
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldSchema }, m_schema);
            }

            return jsonObject;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ci::MidiResourceListEntry MidiResourceListEntry::FromJson(json::JsonObject const& jsonObject) noexcept
    {
        auto entry = winrt::make_self<implementation::MidiResourceListEntry>();

        try
        {
            if (jsonObject == nullptr)
            {
                return *entry;
            }

            winrt::hstring const resourceName{ FieldResource };

            if (jsonObject.HasKey(resourceName) &&
                jsonObject.Lookup(resourceName).ValueType() == json::JsonValueType::String)
            {
                entry->Resource(jsonObject.Lookup(resourceName).GetString());
            }

            entry->CanGet(ReadBoolean(jsonObject, FieldCanGet, true));
            entry->CanSubscribe(ReadBoolean(jsonObject, FieldCanSubscribe, false));
            entry->CanPaginate(ReadBoolean(jsonObject, FieldCanPaginate, false));
            entry->RequireResourceId(ReadBoolean(jsonObject, FieldRequireResId, false));

            winrt::hstring const canSetName{ FieldCanSet };

            if (jsonObject.HasKey(canSetName) &&
                jsonObject.Lookup(canSetName).ValueType() == json::JsonValueType::String)
            {
                entry->CanSet(jsonObject.Lookup(canSetName).GetString());
            }

            ReadStringArray(jsonObject, FieldMediaTypes, entry->MediaTypes());
            ReadStringArray(jsonObject, FieldEncodings, entry->Encodings());

            winrt::hstring const schemaName{ FieldSchema };

            if (jsonObject.HasKey(schemaName) &&
                jsonObject.Lookup(schemaName).ValueType() == json::JsonValueType::Object)
            {
                entry->Schema(jsonObject.Lookup(schemaName).GetObject());
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *entry;
    }

    winrt::hstring MidiResourceListEntry::ToString()
    {
        return m_resource;
    }
}
