// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiProgramListEntry.h"
#include "CapabilityInquiry.MidiProgramListEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        // Resource field names, from M2-107-UM. Spelled once so a reader and a writer cannot drift.
        constexpr std::wstring_view FieldTitle{ L"title" };
        constexpr std::wstring_view FieldBankPC{ L"bankPC" };
        constexpr std::wstring_view FieldTags{ L"tags" };
        constexpr std::wstring_view FieldCategory{ L"category" };

        // A device is free to send a single string where the resource allows an array, so both
        // shapes are accepted rather than one of them being treated as malformed.
        void ReadStringOrArray(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ foundation::Collections::IVector<winrt::hstring> const& destination) noexcept
        {
            try
            {
                winrt::hstring const name{ field };

                if (!jsonObject.HasKey(name))
                {
                    return;
                }

                auto const value = jsonObject.Lookup(name);

                if (value.ValueType() == json::JsonValueType::String)
                {
                    destination.Append(value.GetString());
                }
                else if (value.ValueType() == json::JsonValueType::Array)
                {
                    for (auto const& element : value.GetArray())
                    {
                        if (element.ValueType() == json::JsonValueType::String)
                        {
                            destination.Append(element.GetString());
                        }
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }

        void WriteArrayIfAny(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ foundation::Collections::IVector<winrt::hstring> const& source) noexcept
        {
            try
            {
                if (source.Size() == 0)
                {
                    return;
                }

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

    _Use_decl_annotations_
    MidiProgramListEntry::MidiProgramListEntry(
        winrt::hstring const& title,
        uint8_t const bankMsb,
        uint8_t const bankLsb,
        uint8_t const programChange) noexcept
        : m_title(title)
        , m_bankMsb(bankMsb & 0x7F)
        , m_bankLsb(bankLsb & 0x7F)
        , m_programChange(programChange & 0x7F)
    {
    }

    json::JsonObject MidiProgramListEntry::GetJson() noexcept
    {
        try
        {
            json::JsonObject jsonObject{};

            jsonObject.SetNamedValue(winrt::hstring{ FieldTitle },
                json::JsonValue::CreateStringValue(m_title));

            json::JsonArray bankPC{};
            bankPC.Append(json::JsonValue::CreateNumberValue(m_bankMsb));
            bankPC.Append(json::JsonValue::CreateNumberValue(m_bankLsb));
            bankPC.Append(json::JsonValue::CreateNumberValue(m_programChange));

            jsonObject.SetNamedValue(winrt::hstring{ FieldBankPC }, bankPC);

            WriteArrayIfAny(jsonObject, FieldTags, m_tags);
            WriteArrayIfAny(jsonObject, FieldCategory, m_categories);

            // CollectionTitle is deliberately absent. It is not part of the resource; it records
            // which list the entry came from so a merged view can still say where each row is from.

            return jsonObject;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ci::MidiProgramListEntry MidiProgramListEntry::FromJson(json::JsonObject const& jsonObject) noexcept
    {
        auto entry = winrt::make_self<implementation::MidiProgramListEntry>();

        try
        {
            if (jsonObject == nullptr)
            {
                return *entry;
            }

            winrt::hstring const titleName{ FieldTitle };

            if (jsonObject.HasKey(titleName) &&
                jsonObject.Lookup(titleName).ValueType() == json::JsonValueType::String)
            {
                entry->Title(jsonObject.Lookup(titleName).GetString());
            }

            winrt::hstring const bankName{ FieldBankPC };

            if (jsonObject.HasKey(bankName) &&
                jsonObject.Lookup(bankName).ValueType() == json::JsonValueType::Array)
            {
                auto const bankPC = jsonObject.Lookup(bankName).GetArray();

                // A short array is taken for what it has rather than rejected: the values are
                // independent, and a truncated entry is still worth showing.
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

            ReadStringOrArray(jsonObject, FieldTags, entry->Tags());
            ReadStringOrArray(jsonObject, FieldCategory, entry->Categories());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *entry;
    }

    winrt::hstring MidiProgramListEntry::ToString()
    {
        try
        {
            return winrt::hstring{
                m_title + L" (" +
                winrt::to_hstring(m_bankMsb) + L", " +
                winrt::to_hstring(m_bankLsb) + L", " +
                winrt::to_hstring(m_programChange) + L")" };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return m_title;
        }
    }
}
