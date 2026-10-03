// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_send_pacer.h"

// Reads entries saved in the configuration file. The file can be edited by hand, so every read
// checks the type first, and a value of the wrong type is treated as missing, as the service does.
namespace MidiSavedConfigJson
{
    inline json::IJsonValue Find(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ json::JsonValueType const type) noexcept
    {
        try
        {
            if (parent == nullptr)
            {
                return nullptr;
            }

            winrt::hstring const name{ key };

            if (!parent.HasKey(name))
            {
                return nullptr;
            }

            auto const value = parent.Lookup(name);

            return value != nullptr && value.ValueType() == type ? value : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    inline json::JsonObject Object(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Object);

            return value == nullptr ? json::JsonObject{ nullptr } : value.GetObject();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    inline json::JsonArray Array(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Array);

            return value == nullptr ? json::JsonArray{ nullptr } : value.GetArray();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // the objects in an array, skipping anything else
    inline std::vector<json::JsonObject> Objects(_In_ json::JsonArray const& array) noexcept
    {
        std::vector<json::JsonObject> objects{};

        try
        {
            if (array == nullptr)
            {
                return objects;
            }

            for (uint32_t i = 0; i < array.Size(); i++)
            {
                auto const value = array.GetAt(i);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                {
                    objects.push_back(value.GetObject());
                }
            }
        }
        catch (...)
        {
        }

        return objects;
    }

    inline winrt::hstring String(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ winrt::hstring const& defaultValue = {}) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::String);

            return value == nullptr ? defaultValue : value.GetString();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    inline bool Boolean(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ bool const defaultValue) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Boolean);

            return value == nullptr ? defaultValue : value.GetBoolean();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    // False when the value is missing or not a number
    inline bool TryNumber(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _Out_ double& number) noexcept
    {
        number = 0;

        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Number);

            if (value == nullptr)
            {
                return false;
            }

            number = value.GetNumber();

            return true;
        }
        catch (...)
        {
            number = 0;
            return false;
        }
    }

    // A number outside the range is treated as missing, as the service treats it
    inline uint8_t Byte(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ uint8_t const defaultValue,
        _In_ uint8_t const minimumValue,
        _In_ uint8_t const maximumValue) noexcept
    {
        double number{ 0 };

        if (!TryNumber(parent, key, number) || !(number >= minimumValue && number <= maximumValue))
        {
            return defaultValue;
        }

        return static_cast<uint8_t>(number);
    }

    // A send speed limit, a multiple of MIDI 1.0 wire speed, read the way the network transports
    // read it: 0 and anything faster than the fastest limit are no limit, and a value which is
    // not a speed at all is treated as missing
    inline uint32_t SendSpeedLimit(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ uint32_t const defaultValue) noexcept
    {
        double number{ 0 };

        if (!TryNumber(parent, key, number) || !(number >= 0))
        {
            return defaultValue;
        }

        return number > ::WindowsMidiServicesInternal::MidiSendSpeedMaxMultiple ? 0 : static_cast<uint32_t>(number);
    }

    // Digits only, as a port is written. False for anything else, including a value too large.
    inline bool TryParsePort(_In_ std::wstring const& text, _Out_ uint16_t& port) noexcept
    {
        port = 0;

        if (text.empty() || text.size() > 5)
        {
            return false;
        }

        uint32_t value{ 0 };

        for (auto const ch : text)
        {
            if (ch < L'0' || ch > L'9')
            {
                return false;
            }

            value = (value * 10) + static_cast<uint32_t>(ch - L'0');
        }

        if (value > 0xFFFF)
        {
            return false;
        }

        port = static_cast<uint16_t>(value);

        return true;
    }

    // Entry keys are GUIDs, braced or not
    inline bool TryEntryId(
        _In_ winrt::hstring const& key,
        _Out_ winrt::guid& entryId) noexcept
    {
        entryId = winrt::guid{};

        try
        {
            GUID parsed{};

            if (!internal::TryParseGuidString(internal::TrimmedWStringCopy(std::wstring{ key }), parsed))
            {
                return false;
            }

            entryId = parsed;

            return true;
        }
        catch (...)
        {
            entryId = winrt::guid{};
            return false;
        }
    }

    // The objects stored under a GUID key, keyed the same way, for matching a later change to the
    // entry it belongs to. Two spellings of one key are both kept, in the order the file has them.
    inline std::vector<std::pair<winrt::guid, json::JsonObject>> Entries(_In_ json::JsonObject const& parent) noexcept
    {
        std::vector<std::pair<winrt::guid, json::JsonObject>> entries{};

        try
        {
            if (parent == nullptr)
            {
                return entries;
            }

            for (auto const& pair : parent)
            {
                winrt::guid entryId{};

                if (!TryEntryId(pair.Key(), entryId))
                {
                    continue;
                }

                auto const value = pair.Value();

                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                entries.emplace_back(entryId, value.GetObject());
            }
        }
        catch (...)
        {
        }

        return entries;
    }

    // every object in the list stored under this entry id
    inline std::vector<json::JsonObject> EntriesFor(
        _In_ std::vector<std::pair<winrt::guid, json::JsonObject>> const& entries,
        _In_ winrt::guid const& entryId) noexcept
    {
        std::vector<json::JsonObject> found{};

        try
        {
            for (auto const& [id, entry] : entries)
            {
                if (id == entryId)
                {
                    found.push_back(entry);
                }
            }
        }
        catch (...)
        {
        }

        return found;
    }
}
