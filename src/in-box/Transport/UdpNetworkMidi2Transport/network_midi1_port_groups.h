// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// The groups a Network MIDI 2.0 connection gets MIDI 1.0 ports for when the remote declares no
// function blocks. The transport and the SDK both read and clean the list with these, so the list
// the SDK reads back from a saved entry is the list the transport runs with.
//
// Needs wstring_util.h and windows.h first, which every file including this already has.

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Data.Json.h>

#include "network_json_defs.h"

namespace WindowsMidiServicesInternal
{
    struct MidiNetworkMidi1PortGroupDefinition
    {
        uint8_t GroupIndex{ 0 };

        // Empty means the automatic name, which is the endpoint's name and the group number
        std::wstring Name{};

        bool HasSourcePort{ true };
        bool HasDestinationPort{ true };

        bool operator==(MidiNetworkMidi1PortGroupDefinition const&) const = default;
    };

    using MidiNetworkMidi1PortGroupList = std::vector<MidiNetworkMidi1PortGroupDefinition>;

    // Trimmed, without control characters, and cut to the longest name a MIDI 1.0 app can show.
    // The name comes from a file any user can write, and it ends up in every MIDI 1.0 app.
    inline std::wstring CleanMidiNetworkMidi1PortGroupName(_In_ std::wstring_view const name) noexcept
    {
        try
        {
            std::wstring cleaned{};
            cleaned.reserve(name.size());

            for (auto const ch : name)
            {
                // C0 and C1 control characters, which no port name has a use for
                if (ch < 0x20 || (ch >= 0x7F && ch <= 0x9F)) continue;

                cleaned.push_back(ch);
            }

            cleaned = TrimmedWStringCopy(cleaned);

            if (cleaned.size() > MIDI_NETWORK_MIDI1_PORT_GROUP_NAME_MAX_LENGTH)
            {
                cleaned.resize(MIDI_NETWORK_MIDI1_PORT_GROUP_NAME_MAX_LENGTH);

                // never leave a lead surrogate without its trail
                if (!cleaned.empty() && IS_HIGH_SURROGATE(cleaned.back()))
                {
                    cleaned.pop_back();
                }

                cleaned = TrimmedWStringCopy(cleaned);
            }

            return cleaned;
        }
        catch (...)
        {
            return std::wstring{};
        }
    }

    // One entry per group, sorted by group, with clean names. A later entry for a group replaces
    // an earlier one, and a group with neither port is left out, because it would create nothing.
    inline MidiNetworkMidi1PortGroupList NormalizeMidiNetworkMidi1PortGroups(_In_ MidiNetworkMidi1PortGroupList const& groups) noexcept
    {
        try
        {
            std::map<uint8_t, MidiNetworkMidi1PortGroupDefinition> byGroup{};

            for (auto const& group : groups)
            {
                if (group.GroupIndex >= MIDI_NETWORK_MIDI1_PORT_GROUP_COUNT) continue;

                if (!group.HasSourcePort && !group.HasDestinationPort)
                {
                    byGroup.erase(group.GroupIndex);
                    continue;
                }

                auto cleaned = group;
                cleaned.Name = CleanMidiNetworkMidi1PortGroupName(group.Name);

                byGroup[cleaned.GroupIndex] = std::move(cleaned);
            }

            MidiNetworkMidi1PortGroupList result{};
            result.reserve(byGroup.size());

            for (auto& entry : byGroup)
            {
                result.push_back(std::move(entry.second));
            }

            return result;
        }
        catch (...)
        {
            return MidiNetworkMidi1PortGroupList{};
        }
    }

    // An entry which can't be read is skipped rather than failing the whole list, because the
    // file can be edited by hand. Only the group is required.
    inline MidiNetworkMidi1PortGroupList ReadMidiNetworkMidi1PortGroups(_In_ winrt::Windows::Data::Json::JsonArray const& items) noexcept
    {
        using namespace winrt::Windows::Data::Json;

        try
        {
            MidiNetworkMidi1PortGroupList groups{};

            if (items == nullptr) return groups;

            auto const readBoolean = [](JsonObject const& item, std::wstring_view const key, bool const defaultValue)
                {
                    if (!item.HasKey(key)) return defaultValue;

                    auto const value = item.GetNamedValue(key);

                    return value.ValueType() == JsonValueType::Boolean ? value.GetBoolean() : defaultValue;
                };

            for (uint32_t i = 0; i < items.Size(); i++)
            {
                auto const element = items.GetAt(i);

                // windows.h renames IJsonValue::GetObject, so objects come from GetObjectAt
                if (element == nullptr || element.ValueType() != JsonValueType::Object) continue;

                auto const item = items.GetObjectAt(i);

                if (item == nullptr || !item.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_INDEX_KEY)) continue;

                auto const indexValue = item.GetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_INDEX_KEY);

                if (indexValue.ValueType() != JsonValueType::Number) continue;

                auto const index = indexValue.GetNumber();

                // written this way round so that NaN fails too
                if (!(index >= 0 && index < MIDI_NETWORK_MIDI1_PORT_GROUP_COUNT) || index != std::floor(index)) continue;

                MidiNetworkMidi1PortGroupDefinition group{};
                group.GroupIndex = static_cast<uint8_t>(index);

                if (item.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_NAME_KEY))
                {
                    auto const nameValue = item.GetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_NAME_KEY);

                    if (nameValue.ValueType() == JsonValueType::String)
                    {
                        group.Name = std::wstring{ nameValue.GetString() };
                    }
                }

                group.HasSourcePort = readBoolean(item, MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_HAS_SOURCE_PORT_KEY, true);
                group.HasDestinationPort = readBoolean(item, MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_HAS_DESTINATION_PORT_KEY, true);

                groups.push_back(std::move(group));
            }

            return NormalizeMidiNetworkMidi1PortGroups(groups);
        }
        catch (...)
        {
            return MidiNetworkMidi1PortGroupList{};
        }
    }

    // A transport command carries its arguments as strings, so the list travels as JSON text there
    inline MidiNetworkMidi1PortGroupList ReadMidiNetworkMidi1PortGroups(_In_ std::wstring_view const jsonText) noexcept
    {
        try
        {
            if (jsonText.empty()) return MidiNetworkMidi1PortGroupList{};

            winrt::Windows::Data::Json::JsonArray items{ nullptr };

            if (winrt::Windows::Data::Json::JsonArray::TryParse(winrt::hstring{ jsonText }, items))
            {
                return ReadMidiNetworkMidi1PortGroups(items);
            }
        }
        catch (...)
        {
        }

        return MidiNetworkMidi1PortGroupList{};
    }

    // Every key is written, including the defaults, so a saved entry says exactly what it does.
    // Can throw, as anything building JSON can.
    inline winrt::Windows::Data::Json::JsonArray WriteMidiNetworkMidi1PortGroups(_In_ MidiNetworkMidi1PortGroupList const& groups)
    {
        using namespace winrt::Windows::Data::Json;

        JsonArray items{};

        for (auto const& group : groups)
        {
            JsonObject item{};

            item.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_INDEX_KEY,
                JsonValue::CreateNumberValue(static_cast<double>(group.GroupIndex)));

            item.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_NAME_KEY,
                JsonValue::CreateStringValue(group.Name));

            item.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_HAS_SOURCE_PORT_KEY,
                JsonValue::CreateBooleanValue(group.HasSourcePort));

            item.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_PORT_GROUP_HAS_DESTINATION_PORT_KEY,
                JsonValue::CreateBooleanValue(group.HasDestinationPort));

            items.Append(item);
        }

        return items;
    }
}
