// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiResourceLink.h"
#include "CapabilityInquiry.MidiResourceLink.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        constexpr std::wstring_view FieldResource{ L"resource" };
        constexpr std::wstring_view FieldResourceId{ L"resId" };
        constexpr std::wstring_view FieldTitle{ L"title" };

        winrt::hstring ReadString(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field) noexcept
        {
            try
            {
                winrt::hstring const name{ field };

                if (jsonObject.HasKey(name) &&
                    jsonObject.Lookup(name).ValueType() == json::JsonValueType::String)
                {
                    return jsonObject.Lookup(name).GetString();
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return {};
        }
    }

    json::JsonObject MidiResourceLink::GetJson() noexcept
    {
        try
        {
            json::JsonObject jsonObject{};

            jsonObject.SetNamedValue(winrt::hstring{ FieldResource },
                json::JsonValue::CreateStringValue(m_resource));

            // Both are omitted when empty rather than written as empty strings: a resource
            // identifier that is present but blank is not the same request as one that is absent.
            if (!m_resourceId.empty())
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldResourceId },
                    json::JsonValue::CreateStringValue(m_resourceId));
            }

            if (!m_title.empty())
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldTitle },
                    json::JsonValue::CreateStringValue(m_title));
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
    ci::MidiResourceLink MidiResourceLink::FromJson(json::JsonObject const& jsonObject) noexcept
    {
        auto link = winrt::make_self<implementation::MidiResourceLink>();

        try
        {
            if (jsonObject != nullptr)
            {
                link->Resource(ReadString(jsonObject, FieldResource));
                link->ResourceId(ReadString(jsonObject, FieldResourceId));
                link->Title(ReadString(jsonObject, FieldTitle));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *link;
    }

    winrt::hstring MidiResourceLink::ToString()
    {
        try
        {
            if (m_resourceId.empty())
            {
                return m_resource;
            }

            return winrt::hstring{ m_resource + L":" + m_resourceId };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return m_resource;
        }
    }
}
