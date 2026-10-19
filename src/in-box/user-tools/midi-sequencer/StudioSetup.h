// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What Studio setup lists: each device and group the sequence plays to or records from, and what
// the sequencer does with each one. Whether it plays to it or records from it at all, and what it
// sends besides the notes. Those choices belong to the PC, because they describe the studio rather
// than the music, so every sequence uses them. No XAML here, so the tests compile it unchanged.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // What a device and group gets besides its notes.
    struct DeviceSync
    {
        bool Clock{ false };
        bool StartStop{ false };
        bool SongPosition{ false };

        // A MIDI 2.0 Set Tempo message. A group that speaks MIDI 1.0 never gets it.
        bool Tempo{ false };

        // Positive sends later, negative sends earlier.
        int32_t OffsetMilliseconds{ 0 };

        bool AnyChosen() const noexcept { return Clock || StartStop || SongPosition || Tempo; }

        bool operator==(DeviceSync const&) const = default;
    };

    // A device and group with its choices, as the PC's settings keep them.
    struct DeviceSyncSetting
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };
        DeviceSync Sync{};
    };

    // A device's group the sequencer leaves alone one way or both, so a device that misbehaves
    // when it's sent to, or that sends what it shouldn't, can stay plugged in.
    struct PortSwitch
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };
        bool PlayTo{ true };
        bool RecordFrom{ true };

        bool AnyOff() const noexcept { return !PlayTo || !RecordFrom; }
    };

    // One row of Studio setup.
    struct StudioRow
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };

        // The names of the tracks that play to it, and of those that record from it, in the order
        // the sequence lists them.
        std::vector<std::wstring> Tracks{};
        std::vector<std::wstring> RecordingTracks{};

        bool Metronome{ false };
        bool TimeCode{ false };

        // The sequence follows its clock.
        bool SetsTempo{ false };

        // Nothing in the sequence uses it. Only the PC's settings name it.
        bool OnlySaved{ false };

        DeviceSync Sync{};

        // Turned off in Studio setup: the sequencer doesn't play to it, or doesn't record from it.
        bool PlayToOff{ false };
        bool RecordFromOff{ false };

        bool PlayedTo() const noexcept { return !Tracks.empty() || Metronome || TimeCode; }
        bool RecordedFrom() const noexcept { return !RecordingTracks.empty() || SetsTempo; }
    };

    // Says which saved endpoints are the same device: two endpoints with the same key are. The
    // app answers with the id of the endpoint that's here, and the tests compare saved ids.
    using EndpointKey = std::function<std::wstring(EndpointRef const& endpoint)>;

    // The groups a device sends on, one bit each, for a track that records from any of them. 0
    // when that isn't known, such as for a device that isn't here.
    using SourceGroupsOf = std::function<uint16_t(EndpointRef const& endpoint)>;

    // What the PC's settings say, and what else uses a device.
    struct StudioInputs
    {
        std::vector<DeviceSyncSetting> Sync{};
        std::vector<PortSwitch> Ports{};

        // An empty endpoint means that one is off.
        EndpointRef Metronome{};
        uint8_t MetronomeGroup{ 0 };
        EndpointRef TimeCode{};
        uint8_t TimeCodeGroup{ 0 };
        EndpointRef FollowedClock{};
        uint8_t FollowedClockGroup{ 0 };
    };

    // One row for each device and group, in this order: what the tracks play to and record from,
    // in track order; the metronome; the clock the sequence follows; the devices only the saved
    // choices name; and where time code goes. A saved choice goes on the row it names, and the
    // first one wins when two name the same row.
    std::vector<StudioRow> StudioRows(
        _In_ Sequence const& sequence,
        _In_ StudioInputs const& inputs,
        _In_ EndpointKey const& key,
        _In_ SourceGroupsOf const& sourceGroups);

    // The sync choices to keep: one for each device and group that gets anything, the first row
    // winning when two name the same one.
    std::vector<DeviceSyncSetting> StudioChoices(
        _In_ std::vector<StudioRow> const& rows,
        _In_ EndpointKey const& key);

    // The groups turned off to keep: one for each device and group turned off one way or both.
    std::vector<PortSwitch> StudioPorts(
        _In_ std::vector<StudioRow> const& rows,
        _In_ EndpointKey const& key);

    // The groups of a device turned off for playing to it, or for recording from it, one bit each.
    uint16_t GroupsOff(
        _In_ std::vector<PortSwitch> const& ports,
        _In_ EndpointRef const& endpoint,
        _In_ bool playTo,
        _In_ EndpointKey const& key);

    // The group a Universal MIDI Packet is for, or -1 for one that has none, such as a utility or
    // stream message.
    int32_t MessageGroup(_In_ uint32_t word0) noexcept;
}
