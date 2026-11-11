// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SourceTable.h"
#include "MonitorPalette.h"
#include "StringResources.h"

namespace midi2monitor
{
    namespace
    {
        struct SourceEntry
        {
            std::wstring EndpointDeviceId{};
            winrt::hstring Name{};
            std::array<winrt::hstring, 16> GroupNames{};

            // 17 entries: 0 is for messages without a group, 1-16 the groups. -1 is unassigned.
            std::array<int32_t, 17> Colors{};
        };

        std::mutex g_lock{};
        std::vector<SourceEntry> g_entries{};
        uint32_t g_nextColor{ 0 };

        bool SameId(std::wstring const& left, std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }
    }

    _Use_decl_annotations_
    uint16_t SourceTable::SlotFor(std::wstring const& endpointDeviceId, winrt::hstring const& displayName) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard{ g_lock };

            for (size_t i = 0; i < g_entries.size(); i++)
            {
                if (SameId(g_entries[i].EndpointDeviceId, endpointDeviceId))
                {
                    if (!displayName.empty())
                    {
                        g_entries[i].Name = displayName;
                    }

                    return static_cast<uint16_t>(i);
                }
            }

            if (g_entries.size() >= MaximumSourceSlots)
            {
                return NoSourceSlot;
            }

            SourceEntry entry{};
            entry.EndpointDeviceId = endpointDeviceId;
            entry.Name = displayName;
            entry.Colors.fill(-1);

            g_entries.push_back(std::move(entry));

            return static_cast<uint16_t>(g_entries.size() - 1);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to add a source.")

        return NoSourceSlot;
    }

    _Use_decl_annotations_
    uint16_t SourceTable::FindSlot(std::wstring const& endpointDeviceId) noexcept
    {
        std::lock_guard<std::mutex> guard{ g_lock };

        for (size_t i = 0; i < g_entries.size(); i++)
        {
            if (SameId(g_entries[i].EndpointDeviceId, endpointDeviceId))
            {
                return static_cast<uint16_t>(i);
            }
        }

        return NoSourceSlot;
    }

    _Use_decl_annotations_
    std::wstring SourceTable::EndpointDeviceId(uint16_t slot) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard{ g_lock };
            return slot < g_entries.size() ? g_entries[slot].EndpointDeviceId : std::wstring{};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring SourceTable::EndpointName(uint16_t slot) noexcept
    {
        std::lock_guard<std::mutex> guard{ g_lock };
        return slot < g_entries.size() ? g_entries[slot].Name : winrt::hstring{};
    }

    _Use_decl_annotations_
    void SourceTable::GroupName(uint16_t slot, uint8_t groupNumber, winrt::hstring const& name) noexcept
    {
        std::lock_guard<std::mutex> guard{ g_lock };

        if (slot < g_entries.size() && groupNumber >= 1 && groupNumber <= 16)
        {
            g_entries[slot].GroupNames[groupNumber - 1] = name;
        }
    }

    _Use_decl_annotations_
    winrt::hstring SourceTable::GroupName(uint16_t slot, uint8_t groupNumber) noexcept
    {
        std::lock_guard<std::mutex> guard{ g_lock };

        if (slot < g_entries.size() && groupNumber >= 1 && groupNumber <= 16)
        {
            return g_entries[slot].GroupNames[groupNumber - 1];
        }

        return {};
    }

    _Use_decl_annotations_
    uint32_t SourceTable::ColorIndex(uint16_t slot, uint8_t groupNumber) noexcept
    {
        std::lock_guard<std::mutex> guard{ g_lock };

        if (slot >= g_entries.size() || groupNumber > 16)
        {
            return 0;
        }

        auto& color = g_entries[slot].Colors[groupNumber];

        if (color < 0)
        {
            color = static_cast<int32_t>(g_nextColor++ % MonitorPalette::SourceColorCount);
        }

        return static_cast<uint32_t>(color);
    }

    _Use_decl_annotations_
    winrt::hstring SourceTable::GroupText(uint16_t slot, uint8_t groupNumber) noexcept
    {
        try
        {
            if (groupNumber == 0)
            {
                return {};
            }

            auto name = GroupName(slot, groupNumber);

            // a group named after its endpoint: the From column already says the name
            auto const endpoint = EndpointName(slot);

            if (::CompareStringOrdinal(name.c_str(), -1, endpoint.c_str(), -1, TRUE) == CSTR_EQUAL)
            {
                name = {};
            }

            return name.empty()
                ? winrt::hstring{ std::format(L"{}", groupNumber) }
                : resources::FormatString(L"SourceGroupNamedFormat", groupNumber, std::wstring{ name });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to describe a source group.")

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring SourceTable::Describe(uint16_t slot, uint8_t groupNumber) noexcept
    {
        try
        {
            auto const name = EndpointName(slot);
            auto const group = GroupText(slot, groupNumber);

            if (group.empty())
            {
                return name;
            }

            return resources::FormatString(L"SourceDescriptionFormat", std::wstring{ name }, std::wstring{ group });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to describe a source.")

        return {};
    }
}
