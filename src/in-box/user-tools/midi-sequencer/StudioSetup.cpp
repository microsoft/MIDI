// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StudioSetup.h"

#include <unordered_map>
#include <unordered_set>

namespace midisequencer
{
    namespace
    {
        std::wstring RowKey(EndpointKey const& key, EndpointRef const& endpoint, uint8_t const group)
        {
            return key(endpoint) + L"#" + std::to_wstring(group & 0x0F);
        }
    }

    _Use_decl_annotations_
    std::vector<StudioRow> StudioRows(
        Sequence const& sequence,
        StudioInputs const& inputs,
        EndpointKey const& key,
        SourceGroupsOf const& sourceGroups)
    {
        std::vector<StudioRow> rows{};
        std::unordered_map<std::wstring, size_t> index{};

        // The row for a device and group, made at the end when there isn't one. Use it before
        // asking for another: making one can move the rest.
        auto const rowFor = [&](EndpointRef const& endpoint, uint8_t const group) -> StudioRow&
        {
            auto const rowKey = RowKey(key, endpoint, group);

            if (auto const found = index.find(rowKey); found != index.end())
            {
                return rows[found->second];
            }

            StudioRow row{};
            row.Endpoint = endpoint;
            row.Group = static_cast<uint8_t>(group & 0x0F);

            index.emplace(rowKey, rows.size());
            rows.push_back(std::move(row));
            return rows.back();
        };

        ForEachTrack(sequence, [&](Track const& track, size_t)
        {
            if (track.IsFolder)
            {
                return true;
            }

            if (!track.Destination.Endpoint.IsEmpty())
            {
                rowFor(track.Destination.Endpoint, track.Destination.Group).Tracks.push_back(track.Name);
            }

            if (!track.Source.Endpoint.IsEmpty())
            {
                if (track.Source.Group >= 0)
                {
                    rowFor(track.Source.Endpoint, static_cast<uint8_t>(track.Source.Group)).RecordingTracks.push_back(track.Name);
                }
                else
                {
                    // Any group: each one the device sends on, or the first when that isn't known.
                    auto groups = sourceGroups ? sourceGroups(track.Source.Endpoint) : uint16_t{ 0 };

                    if (groups == 0)
                    {
                        groups = 1;
                    }

                    for (uint8_t group = 0; group < 16; ++group)
                    {
                        if ((groups & (1u << group)) != 0)
                        {
                            rowFor(track.Source.Endpoint, group).RecordingTracks.push_back(track.Name);
                        }
                    }
                }
            }

            return true;
        });

        if (!inputs.Metronome.IsEmpty())
        {
            rowFor(inputs.Metronome, inputs.MetronomeGroup).Metronome = true;
        }

        if (!inputs.FollowedClock.IsEmpty())
        {
            rowFor(inputs.FollowedClock, inputs.FollowedClockGroup).SetsTempo = true;
        }

        std::unordered_set<std::wstring> applied{};

        for (auto const& setting : inputs.Sync)
        {
            if (setting.Endpoint.IsEmpty())
            {
                continue;
            }

            auto const rowKey = RowKey(key, setting.Endpoint, setting.Group);

            if (!applied.insert(rowKey).second)
            {
                continue;
            }

            auto const existed = index.contains(rowKey);
            auto& row = rowFor(setting.Endpoint, setting.Group);
            row.Sync = setting.Sync;

            if (!existed)
            {
                row.OnlySaved = true;
            }
        }

        applied.clear();

        for (auto const& port : inputs.Ports)
        {
            if (port.Endpoint.IsEmpty() || !port.AnyOff())
            {
                continue;
            }

            auto const rowKey = RowKey(key, port.Endpoint, port.Group);

            if (!applied.insert(rowKey).second)
            {
                continue;
            }

            auto const existed = index.contains(rowKey);
            auto& row = rowFor(port.Endpoint, port.Group);
            row.PlayToOff = !port.PlayTo;
            row.RecordFromOff = !port.RecordFrom;

            if (!existed)
            {
                row.OnlySaved = true;
            }
        }

        if (!inputs.TimeCode.IsEmpty())
        {
            rowFor(inputs.TimeCode, inputs.TimeCodeGroup).TimeCode = true;
        }

        return rows;
    }

    _Use_decl_annotations_
    std::vector<DeviceSyncSetting> StudioChoices(std::vector<StudioRow> const& rows, EndpointKey const& key)
    {
        std::vector<DeviceSyncSetting> choices{};
        std::unordered_set<std::wstring> seen{};

        for (auto const& row : rows)
        {
            if (row.Endpoint.IsEmpty() || !row.Sync.AnyChosen())
            {
                continue;
            }

            if (!seen.insert(RowKey(key, row.Endpoint, row.Group)).second)
            {
                continue;
            }

            choices.push_back(DeviceSyncSetting{ row.Endpoint, static_cast<uint8_t>(row.Group & 0x0F), row.Sync });
        }

        return choices;
    }

    _Use_decl_annotations_
    std::vector<PortSwitch> StudioPorts(std::vector<StudioRow> const& rows, EndpointKey const& key)
    {
        std::vector<PortSwitch> ports{};
        std::unordered_set<std::wstring> seen{};

        for (auto const& row : rows)
        {
            if (row.Endpoint.IsEmpty() || (!row.PlayToOff && !row.RecordFromOff))
            {
                continue;
            }

            if (!seen.insert(RowKey(key, row.Endpoint, row.Group)).second)
            {
                continue;
            }

            ports.push_back(PortSwitch{ row.Endpoint, static_cast<uint8_t>(row.Group & 0x0F), !row.PlayToOff, !row.RecordFromOff });
        }

        return ports;
    }

    _Use_decl_annotations_
    uint16_t GroupsOff(std::vector<PortSwitch> const& ports, EndpointRef const& endpoint, bool playTo, EndpointKey const& key)
    {
        if (endpoint.IsEmpty() || ports.empty())
        {
            return 0;
        }

        auto const wanted = key(endpoint);
        uint16_t seen{ 0 };
        uint16_t off{ 0 };

        // The first one for a group wins, the way Studio setup shows them.
        for (auto const& port : ports)
        {
            auto const bit = static_cast<uint16_t>(1u << (port.Group & 0x0F));

            if ((seen & bit) != 0 || key(port.Endpoint) != wanted)
            {
                continue;
            }

            seen = static_cast<uint16_t>(seen | bit);

            if (playTo ? !port.PlayTo : !port.RecordFrom)
            {
                off = static_cast<uint16_t>(off | bit);
            }
        }

        return off;
    }

    _Use_decl_annotations_
    int32_t MessageGroup(uint32_t word0) noexcept
    {
        switch (word0 >> 28)
        {
        case 0x1:   // system common and real time
        case 0x2:   // MIDI 1.0 channel voice
        case 0x3:   // data, including system exclusive
        case 0x4:   // MIDI 2.0 channel voice
        case 0x5:   // data 128
        case 0xD:   // flex data
            return static_cast<int32_t>((word0 >> 24) & 0x0F);
        default:
            return -1;
        }
    }
}
