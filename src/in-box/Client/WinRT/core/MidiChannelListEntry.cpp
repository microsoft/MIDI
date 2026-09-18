// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiChannelListEntry.h"
#include "MidiResourceLink.h"
#include "CapabilityInquiry.MidiChannelListEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        constexpr std::wstring_view FieldTitle{ L"title" };
        constexpr std::wstring_view FieldChannel{ L"channel" };
        constexpr std::wstring_view FieldBankPC{ L"bankPC" };
        constexpr std::wstring_view FieldProgramTitle{ L"programTitle" };
        constexpr std::wstring_view FieldLinks{ L"links" };

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

    json::JsonObject MidiChannelListEntry::GetJson() noexcept
    {
        try
        {
            json::JsonObject jsonObject{};

            jsonObject.SetNamedValue(winrt::hstring{ FieldTitle },
                json::JsonValue::CreateStringValue(m_title));

            jsonObject.SetNamedValue(winrt::hstring{ FieldChannel },
                json::JsonValue::CreateNumberValue(m_channel));

            json::JsonArray bankPC{};
            bankPC.Append(json::JsonValue::CreateNumberValue(m_bankMsb));
            bankPC.Append(json::JsonValue::CreateNumberValue(m_bankLsb));
            bankPC.Append(json::JsonValue::CreateNumberValue(m_programChange));

            jsonObject.SetNamedValue(winrt::hstring{ FieldBankPC }, bankPC);

            if (!m_programTitle.empty())
            {
                jsonObject.SetNamedValue(winrt::hstring{ FieldProgramTitle },
                    json::JsonValue::CreateStringValue(m_programTitle));
            }

            if (m_links.Size() > 0)
            {
                json::JsonArray links{};

                for (auto const& link : m_links)
                {
                    if (link == nullptr) continue;

                    auto const linkJson = link.GetJson();

                    if (linkJson != nullptr)
                    {
                        links.Append(linkJson);
                    }
                }

                jsonObject.SetNamedValue(winrt::hstring{ FieldLinks }, links);
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
    ci::MidiChannelListEntry MidiChannelListEntry::FromJson(json::JsonObject const& jsonObject) noexcept
    {
        auto entry = winrt::make_self<implementation::MidiChannelListEntry>();

        try
        {
            if (jsonObject == nullptr)
            {
                return *entry;
            }

            entry->Title(ReadString(jsonObject, FieldTitle));
            entry->ProgramTitle(ReadString(jsonObject, FieldProgramTitle));

            winrt::hstring const channelName{ FieldChannel };

            if (jsonObject.HasKey(channelName) &&
                jsonObject.Lookup(channelName).ValueType() == json::JsonValueType::Number)
            {
                auto const value = jsonObject.Lookup(channelName).GetNumber();

                // The list numbers channels from 1 and stops at 256. A value outside that says
                // nothing useful, so the entry keeps its default rather than carrying a channel
                // number no caller could act on.
                if (std::isfinite(value) && value >= 1.0 && value <= 256.0)
                {
                    entry->Channel(static_cast<uint16_t>(value));
                }
            }

            winrt::hstring const bankName{ FieldBankPC };

            if (jsonObject.HasKey(bankName) &&
                jsonObject.Lookup(bankName).ValueType() == json::JsonValueType::Array)
            {
                auto const bankPC = jsonObject.Lookup(bankName).GetArray();

                auto const numberAt = [&bankPC](uint32_t const index) -> uint8_t
                    {
                        if (index >= bankPC.Size()) return 0;
                        if (bankPC.GetAt(index).ValueType() != json::JsonValueType::Number) return 0;

                        auto const value = bankPC.GetAt(index).GetNumber();

                        if (!std::isfinite(value) || value < 0.0 || value > 127.0) return 0;

                        return static_cast<uint8_t>(value);
                    };

                entry->BankMsb(numberAt(0));
                entry->BankLsb(numberAt(1));
                entry->ProgramChange(numberAt(2));
            }

            winrt::hstring const linksName{ FieldLinks };

            if (jsonObject.HasKey(linksName) &&
                jsonObject.Lookup(linksName).ValueType() == json::JsonValueType::Array)
            {
                for (auto const& element : jsonObject.Lookup(linksName).GetArray())
                {
                    if (element.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    entry->Links().Append(
                        implementation::MidiResourceLink::FromJson(element.GetObject()));
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *entry;
    }

    winrt::hstring MidiChannelListEntry::ToString()
    {
        try
        {
            return winrt::hstring{ winrt::to_hstring(m_channel) + L": " + m_title };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return m_title;
        }
    }
}
