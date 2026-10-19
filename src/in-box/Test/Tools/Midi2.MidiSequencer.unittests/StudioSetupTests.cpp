// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StudioSetupTests.h"

#include "StudioSetup.h"

#include <cwctype>
#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    // Ids compared without regard to case, as the app compares them. A device saved without an
    // id goes by its name.
    std::wstring KeyOf(EndpointRef const& endpoint)
    {
        if (endpoint.Id.empty())
        {
            return L"name:" + endpoint.Name;
        }

        std::wstring key{ endpoint.Id };

        for (auto& ch : key)
        {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }

        return key;
    }

    // Nothing known about which groups any device sends on.
    uint16_t NoGroups(EndpointRef const&)
    {
        return 0;
    }

    EndpointRef Device(std::wstring const& id)
    {
        return EndpointRef{ L"Device " + id, id };
    }

    Track TrackTo(std::wstring const& name, std::wstring const& deviceId, uint8_t group)
    {
        Track track{};
        track.Id = L"t-" + name;
        track.Name = name;

        if (!deviceId.empty())
        {
            track.Destination.Endpoint = Device(deviceId);
        }

        track.Destination.Group = group;
        return track;
    }

    DeviceSyncSetting Saved(std::wstring const& deviceId, uint8_t group, DeviceSync sync)
    {
        return DeviceSyncSetting{ Device(deviceId), group, sync };
    }

    DeviceSync ClockOnly()
    {
        DeviceSync sync{};
        sync.Clock = true;
        return sync;
    }

    StudioInputs SyncInputs(std::vector<DeviceSyncSetting> sync)
    {
        StudioInputs inputs{};
        inputs.Sync = std::move(sync);
        return inputs;
    }

    // Bass, Lead and Pad, a folder holding Keys, then Drums and a track that plays nowhere.
    Sequence Band()
    {
        Sequence sequence{};
        sequence.Tracks.push_back(TrackTo(L"Bass", L"device-a", 0));
        sequence.Tracks.push_back(TrackTo(L"Lead", L"device-b", 0));
        sequence.Tracks.push_back(TrackTo(L"Pad", L"DEVICE-A", 0));

        Track folder{};
        folder.Id = L"f-keys";
        folder.Name = L"Keys folder";
        folder.IsFolder = true;
        folder.Children.push_back(TrackTo(L"Keys", L"device-b", 0));
        sequence.Tracks.push_back(folder);

        sequence.Tracks.push_back(TrackTo(L"Drums", L"device-a", 1));
        sequence.Tracks.push_back(TrackTo(L"Sketch", L"", 0));
        return sequence;
    }
}

void StudioSetupTests::ARowForEachDeviceAndGroupInTrackOrder()
{
    auto const rows = StudioRows(Band(), {}, KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 3 }, rows.size());

    VERIFY_ARE_EQUAL(std::wstring{ L"device-a" }, rows[0].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, rows[0].Group);
    VERIFY_ARE_EQUAL(size_t{ 2 }, rows[0].Tracks.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Bass" }, rows[0].Tracks[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"Pad" }, rows[0].Tracks[1]);

    VERIFY_ARE_EQUAL(std::wstring{ L"device-b" }, rows[1].Endpoint.Id);
    VERIFY_ARE_EQUAL(size_t{ 2 }, rows[1].Tracks.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Keys" }, rows[1].Tracks[1]);

    // Another group of the same device is another row: behind an interface it's another cable.
    VERIFY_ARE_EQUAL(std::wstring{ L"device-a" }, rows[2].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, rows[2].Group);

    for (auto const& row : rows)
    {
        VERIFY_IS_TRUE(row.PlayedTo());
        VERIFY_IS_FALSE(row.RecordedFrom());
        VERIFY_IS_FALSE(row.OnlySaved);
        VERIFY_IS_FALSE(row.Sync.AnyChosen());
        VERIFY_IS_FALSE(row.PlayToOff);
        VERIFY_IS_FALSE(row.RecordFromOff);
    }
}

void StudioSetupTests::SourcesGetRowsToo()
{
    Sequence sequence{};

    auto keys = TrackTo(L"Keys", L"synth", 0);
    keys.Source.Endpoint = Device(L"controller");
    keys.Source.Group = 2;
    sequence.Tracks.push_back(keys);

    // Any group of an interface that sends on groups 1 and 3.
    auto lead = TrackTo(L"Lead", L"synth", 0);
    lead.Source.Endpoint = Device(L"interface");
    lead.Source.Group = -1;
    sequence.Tracks.push_back(lead);

    // The same interface's group 3 is played to, so it shares that row.
    sequence.Tracks.push_back(TrackTo(L"Pad", L"interface", 2));

    StudioInputs inputs{};
    inputs.FollowedClock = Device(L"drum-machine");
    inputs.FollowedClockGroup = 0;

    auto const rows = StudioRows(sequence, inputs, KeyOf, [](EndpointRef const& endpoint) -> uint16_t
    {
        return endpoint.Id == L"interface" ? uint16_t{ 0b0101 } : uint16_t{ 0 };
    });

    VERIFY_ARE_EQUAL(size_t{ 5 }, rows.size());

    VERIFY_ARE_EQUAL(std::wstring{ L"synth" }, rows[0].Endpoint.Id);
    VERIFY_ARE_EQUAL(size_t{ 2 }, rows[0].Tracks.size());
    VERIFY_IS_FALSE(rows[0].RecordedFrom());

    VERIFY_ARE_EQUAL(std::wstring{ L"controller" }, rows[1].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, rows[1].Group);
    VERIFY_ARE_EQUAL(std::wstring{ L"Keys" }, rows[1].RecordingTracks[0]);
    VERIFY_IS_FALSE(rows[1].PlayedTo());

    VERIFY_ARE_EQUAL(std::wstring{ L"interface" }, rows[2].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, rows[2].Group);
    VERIFY_ARE_EQUAL(std::wstring{ L"Lead" }, rows[2].RecordingTracks[0]);

    VERIFY_ARE_EQUAL(std::wstring{ L"interface" }, rows[3].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, rows[3].Group);
    VERIFY_ARE_EQUAL(std::wstring{ L"Lead" }, rows[3].RecordingTracks[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"Pad" }, rows[3].Tracks[0]);
    VERIFY_IS_TRUE(rows[3].PlayedTo());
    VERIFY_IS_TRUE(rows[3].RecordedFrom());

    VERIFY_ARE_EQUAL(std::wstring{ L"drum-machine" }, rows[4].Endpoint.Id);
    VERIFY_IS_TRUE(rows[4].SetsTempo);
    VERIFY_IS_TRUE(rows[4].RecordedFrom());
    VERIFY_IS_FALSE(rows[4].PlayedTo());

    // Any group of a device whose groups aren't known is its first group.
    Sequence unknown{};
    auto pad = TrackTo(L"Pad", L"", 0);
    pad.Source.Endpoint = Device(L"unplugged");
    pad.Source.Group = -1;
    unknown.Tracks.push_back(pad);

    auto const unknownRows = StudioRows(unknown, {}, KeyOf, NoGroups);
    VERIFY_ARE_EQUAL(size_t{ 1 }, unknownRows.size());
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, unknownRows[0].Group);
}

void StudioSetupTests::SavedChoicesLandOnTheirRows()
{
    DeviceSync startAndClock{};
    startAndClock.Clock = true;
    startAndClock.StartStop = true;

    DeviceSync tempo{};
    tempo.Tempo = true;
    tempo.OffsetMilliseconds = -5;

    auto const rows = StudioRows(Band(), SyncInputs({ Saved(L"DEVICE-B", 0, startAndClock), Saved(L"device-a", 1, tempo) }), KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 3 }, rows.size());
    VERIFY_IS_FALSE(rows[0].Sync.AnyChosen());
    VERIFY_IS_TRUE(rows[1].Sync == startAndClock);
    VERIFY_IS_TRUE(rows[2].Sync == tempo);
    VERIFY_IS_FALSE(rows[1].OnlySaved);
    VERIFY_IS_FALSE(rows[2].OnlySaved);
}

void StudioSetupTests::DevicesOnlyTheSettingsNameComeAfterTheMetronome()
{
    Sequence sequence{};
    sequence.Tracks.push_back(TrackTo(L"Bass", L"device-a", 0));

    auto inputs = SyncInputs({ Saved(L"drum-machine", 2, ClockOnly()) });
    inputs.Metronome = Device(L"click");
    inputs.MetronomeGroup = 9;

    auto const rows = StudioRows(sequence, inputs, KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 3 }, rows.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"device-a" }, rows[0].Endpoint.Id);

    VERIFY_ARE_EQUAL(std::wstring{ L"click" }, rows[1].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, rows[1].Group);
    VERIFY_IS_TRUE(rows[1].Metronome);
    VERIFY_IS_TRUE(rows[1].Tracks.empty());

    VERIFY_ARE_EQUAL(std::wstring{ L"drum-machine" }, rows[2].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, rows[2].Group);
    VERIFY_IS_TRUE(rows[2].OnlySaved);
    VERIFY_IS_TRUE(rows[2].Sync.Clock);
}

void StudioSetupTests::TheMetronomeAndTimeCodeShareTheirDevicesRow()
{
    {
        StudioInputs inputs{};
        inputs.Metronome = Device(L"Device-A");
        inputs.TimeCode = Device(L"device-b");

        auto const rows = StudioRows(Band(), inputs, KeyOf, NoGroups);

        VERIFY_ARE_EQUAL(size_t{ 3 }, rows.size());
        VERIFY_IS_TRUE(rows[0].Metronome);
        VERIFY_IS_TRUE(rows[1].TimeCode);
    }

    // Time code to a device nothing else uses is a row of its own, last, and not one the
    // settings alone name.
    {
        Sequence sequence{};
        sequence.Tracks.push_back(TrackTo(L"Bass", L"device-a", 0));

        auto inputs = SyncInputs({ Saved(L"drum-machine", 0, ClockOnly()) });
        inputs.TimeCode = Device(L"video");
        inputs.TimeCodeGroup = 3;

        auto const rows = StudioRows(sequence, inputs, KeyOf, NoGroups);

        VERIFY_ARE_EQUAL(size_t{ 3 }, rows.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"video" }, rows[2].Endpoint.Id);
        VERIFY_ARE_EQUAL(uint8_t{ 3 }, rows[2].Group);
        VERIFY_IS_TRUE(rows[2].TimeCode);
        VERIFY_IS_FALSE(rows[2].OnlySaved);
    }
}

void StudioSetupTests::TheFirstSavedChoiceForARowWins()
{
    DeviceSync startStop{};
    startStop.StartStop = true;

    Sequence sequence{};
    sequence.Tracks.push_back(TrackTo(L"Bass", L"device-a", 0));

    auto const rows = StudioRows(sequence, SyncInputs({ Saved(L"device-a", 0, ClockOnly()), Saved(L"DEVICE-A", 0, startStop) }), KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 1 }, rows.size());
    VERIFY_IS_TRUE(rows[0].Sync == ClockOnly());
}

void StudioSetupTests::OnlyRowsThatGetSomethingAreKept()
{
    DeviceSync tempo{};
    tempo.Tempo = true;

    // An offset alone changes nothing that's sent, so it isn't worth keeping.
    DeviceSync offsetOnly{};
    offsetOnly.OffsetMilliseconds = 12;

    std::vector<StudioRow> rows(4);
    rows[0].Endpoint = Device(L"device-a");
    rows[0].Sync = ClockOnly();
    rows[1].Endpoint = Device(L"device-b");
    rows[1].Sync = offsetOnly;
    rows[2].Endpoint = Device(L"device-c");
    rows[2].Group = 1;
    rows[2].Sync = tempo;
    rows[3].Endpoint = Device(L"DEVICE-A");
    rows[3].Sync = tempo;

    auto const kept = StudioChoices(rows, KeyOf);

    VERIFY_ARE_EQUAL(size_t{ 2 }, kept.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"device-a" }, kept[0].Endpoint.Id);
    VERIFY_IS_TRUE(kept[0].Sync == ClockOnly());
    VERIFY_ARE_EQUAL(std::wstring{ L"device-c" }, kept[1].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, kept[1].Group);
    VERIFY_IS_TRUE(kept[1].Sync == tempo);
}

void StudioSetupTests::KeptChoicesComeBackOnTheSameRows()
{
    DeviceSync everything{};
    everything.Clock = true;
    everything.StartStop = true;
    everything.SongPosition = true;
    everything.Tempo = true;
    everything.OffsetMilliseconds = 7;

    auto const first = StudioRows(Band(), SyncInputs({ Saved(L"device-b", 0, everything), Saved(L"drum-machine", 4, ClockOnly()) }), KeyOf, NoGroups);
    auto const second = StudioRows(Band(), SyncInputs(StudioChoices(first, KeyOf)), KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(first.size(), second.size());

    for (size_t i = 0; i < first.size(); ++i)
    {
        VERIFY_ARE_EQUAL(first[i].Endpoint.Id, second[i].Endpoint.Id);
        VERIFY_ARE_EQUAL(first[i].Group, second[i].Group);
        VERIFY_ARE_EQUAL(first[i].OnlySaved, second[i].OnlySaved);
        VERIFY_IS_TRUE(first[i].Sync == second[i].Sync);
    }
}

void StudioSetupTests::TurnedOffGroupsComeBackOnTheirRows()
{
    StudioInputs inputs{};
    inputs.Ports = {
        PortSwitch{ Device(L"DEVICE-B"), 0, false, true },      // Lead and Keys play there: don't play to it
        PortSwitch{ Device(L"arduino"), 1, false, false },      // nothing uses it: leave it alone both ways
        PortSwitch{ Device(L"device-a"), 1, true, true } };     // nothing off, so nothing to keep

    auto const rows = StudioRows(Band(), inputs, KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 4 }, rows.size());

    VERIFY_ARE_EQUAL(std::wstring{ L"device-b" }, rows[1].Endpoint.Id);
    VERIFY_IS_TRUE(rows[1].PlayToOff);
    VERIFY_IS_FALSE(rows[1].RecordFromOff);
    VERIFY_IS_FALSE(rows[1].OnlySaved);

    VERIFY_ARE_EQUAL(std::wstring{ L"device-a" }, rows[2].Endpoint.Id);
    VERIFY_IS_FALSE(rows[2].PlayToOff);
    VERIFY_IS_FALSE(rows[2].OnlySaved);

    VERIFY_ARE_EQUAL(std::wstring{ L"arduino" }, rows[3].Endpoint.Id);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, rows[3].Group);
    VERIFY_IS_TRUE(rows[3].PlayToOff);
    VERIFY_IS_TRUE(rows[3].RecordFromOff);
    VERIFY_IS_TRUE(rows[3].OnlySaved);

    auto const kept = StudioPorts(rows, KeyOf);

    // The row the tracks made keeps their name for the device.
    VERIFY_ARE_EQUAL(size_t{ 2 }, kept.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"device-b" }, kept[0].Endpoint.Id);
    VERIFY_IS_FALSE(kept[0].PlayTo);
    VERIFY_IS_TRUE(kept[0].RecordFrom);
    VERIFY_ARE_EQUAL(std::wstring{ L"arduino" }, kept[1].Endpoint.Id);
    VERIFY_IS_FALSE(kept[1].PlayTo);
    VERIFY_IS_FALSE(kept[1].RecordFrom);

    StudioInputs again{};
    again.Ports = kept;

    auto const second = StudioRows(Band(), again, KeyOf, NoGroups);

    VERIFY_ARE_EQUAL(size_t{ 4 }, second.size());
    VERIFY_IS_TRUE(second[1].PlayToOff);
    VERIFY_IS_TRUE(second[3].PlayToOff && second[3].RecordFromOff && second[3].OnlySaved);
}

void StudioSetupTests::GroupsOffSaysWhichGroupsAndWhichWay()
{
    std::vector<PortSwitch> const ports{
        PortSwitch{ Device(L"device-a"), 0, false, true },
        PortSwitch{ Device(L"DEVICE-A"), 3, false, false },
        PortSwitch{ Device(L"device-a"), 5, true, false },
        PortSwitch{ Device(L"device-b"), 0, false, false },
        // A second one for a group is ignored, the way Studio setup ignores it.
        PortSwitch{ Device(L"device-a"), 5, false, true } };

    VERIFY_ARE_EQUAL(uint16_t{ 0b1001 }, GroupsOff(ports, Device(L"Device-A"), true, KeyOf));
    VERIFY_ARE_EQUAL(uint16_t{ 0b101000 }, GroupsOff(ports, Device(L"device-a"), false, KeyOf));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, GroupsOff(ports, Device(L"device-c"), true, KeyOf));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, GroupsOff(ports, EndpointRef{}, true, KeyOf));
}

void StudioSetupTests::MessageGroupIsReadOnlyWhereThereIsOne()
{
    VERIFY_ARE_EQUAL(3, MessageGroup(0x23903C7Fu));     // MIDI 1.0 note on, group 4
    VERIFY_ARE_EQUAL(15, MessageGroup(0x4F903C00u));    // MIDI 2.0 note on, group 16
    VERIFY_ARE_EQUAL(1, MessageGroup(0x11F80000u));     // clock, group 2
    VERIFY_ARE_EQUAL(2, MessageGroup(0x32160000u));     // system exclusive, group 3
    VERIFY_ARE_EQUAL(5, MessageGroup(0x55000000u));     // data 128, group 6
    VERIFY_ARE_EQUAL(7, MessageGroup(0xD7100000u));     // flex data, group 8
    VERIFY_ARE_EQUAL(-1, MessageGroup(0x00000000u));    // utility: no op
    VERIFY_ARE_EQUAL(-1, MessageGroup(0xF0000000u));    // UMP stream
}
