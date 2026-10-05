// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatchSerializerTests.h"
#include "TestMessages.h"

#include "PatchSerializer.h"

#include <algorithm>
#include <random>
#include <string>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t ControlChange = 0xB;

    // A version 1 patch as Preview 10 wrote it: one keyboard, one synth, and a connection that
    // keeps out clock, lets only channels 1 and 2 through, transposes up an octave, and is held
    // to twice MIDI 1.0 wire speed.
    constexpr wchar_t Version1Patch[] = LR"({
        "_comment": "Windows MIDI Patchbay.",
        "fileVersion": 1,
        "name": "Old patch",
        "description": "From Preview 10",
        "activateAtStartup": true,
        "waitForSendComplete": false,
        "endpoints": [
            { "id": "kb", "displayName": "Keyboard", "transportCode": "KS", "match": {}, "matchMode": "endpointDeviceId", "x": 60, "y": 48, "showAllGroups": false },
            { "id": "syn", "displayName": "Synth", "transportCode": "KS", "match": {}, "matchMode": "endpointDeviceId", "x": 460, "y": 48, "showAllGroups": false }
        ],
        "connections": [
            { "id": "c1", "sourceEndpointId": "kb", "sourceGroup": -1, "destinationEndpointId": "syn", "destinationGroup": 2, "muted": false,
              "filter": { "active": true, "channels": 3, "systemMessages": 1007 },
              "transform": { "active": true, "valueScale": "percent", "transposeSemitones": 12 },
              "sendSpeedLimit": 2 }
        ]
    })";

    constexpr wchar_t PlainVersion1Patch[] = LR"({
        "fileVersion": 1,
        "name": "Plain",
        "endpoints": [
            { "id": "kb", "displayName": "Keyboard", "match": {}, "x": 60, "y": 48 },
            { "id": "syn", "displayName": "Synth", "match": {}, "x": 460, "y": 200 }
        ],
        "connections": [
            { "id": "c1", "sourceEndpointId": "kb", "sourceGroup": 0, "destinationEndpointId": "syn", "destinationGroup": -1, "muted": true },
            { "id": "c2", "sourceEndpointId": "kb", "sourceGroup": 0, "destinationEndpointId": "syn", "destinationGroup": -1 }
        ]
    })";

    PatchEndpoint MakeEndpoint(std::wstring const& id)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        return endpoint;
    }

    PatchBlock MakeBlock(std::wstring const& id, BlockKind kind)
    {
        PatchBlock block{};
        block.Id = id;
        block.Kind = kind;
        block.Settings = DefaultBlockSettings(kind);
        return block;
    }

    PatchConnection MakeLink(std::wstring const& id, std::wstring const& from, std::wstring const& to)
    {
        PatchConnection link{};
        link.Id = id;
        link.SourceId = from;
        link.DestinationId = to;
        return link;
    }

    LegacyConnection MakeLegacy()
    {
        LegacyConnection legacy{};
        legacy.Id = L"legacy";
        legacy.SourceEndpointId = L"in";
        legacy.DestinationEndpointId = L"out";
        return legacy;
    }

    PatchDocument TwoEndpoints()
    {
        PatchDocument patch{};
        patch.Endpoints.push_back(MakeEndpoint(L"in"));
        patch.Endpoints.push_back(MakeEndpoint(L"out"));
        return patch;
    }

    // What version 1 did to one message on one connection.
    bool RunVersion1(LegacyConnection const& legacy, Message& message)
    {
        if (!legacy.Filter.Allows(message.Words.data(), message.Count))
        {
            return false;
        }

        legacy.Transform.Apply(message.Words.data(), message.Count);
        return true;
    }

    // Follows the one path a converted connection has, from its source endpoint to where it ends.
    bool RunConvertedChain(PatchDocument const& patch, std::wstring const& sourceId, Message& message, std::wstring& destination)
    {
        auto at = sourceId;

        for (size_t step = 0; step < 64; step++)
        {
            auto const link = std::find_if(patch.Connections.begin(), patch.Connections.end(),
                [&at](PatchConnection const& c) { return c.SourceId == at; });

            if (link == patch.Connections.end())
            {
                return false;
            }

            auto const* block = patch.FindBlock(link->DestinationId);

            if (block == nullptr)
            {
                destination = link->DestinationId;
                return true;
            }

            if (!block->Bypassed && !ProcessBlock(block->Kind, block->Settings, message.Words.data(), message.Count))
            {
                return false;
            }

            at = block->Id;
        }

        return false;
    }

    std::vector<BlockKind> ChainKinds(PatchDocument const& patch, std::wstring const& sourceId)
    {
        std::vector<BlockKind> kinds{};
        auto at = sourceId;

        for (size_t step = 0; step < 64; step++)
        {
            auto const link = std::find_if(patch.Connections.begin(), patch.Connections.end(),
                [&at](PatchConnection const& c) { return c.SourceId == at; });

            if (link == patch.Connections.end())
            {
                break;
            }

            auto const* block = patch.FindBlock(link->DestinationId);

            if (block == nullptr)
            {
                break;
            }

            kinds.push_back(block->Kind);
            at = block->Id;
        }

        return kinds;
    }

    struct Random
    {
        std::mt19937 Engine{ 20261011u };

        int Between(int low, int high)
        {
            return std::uniform_int_distribution<int>(low, high)(Engine);
        }

        bool Chance(int percent)
        {
            return Between(0, 99) < percent;
        }

        uint32_t Word()
        {
            return static_cast<uint32_t>(Engine());
        }
    };

    // Values chosen often enough that the maps and the messages meet.
    constexpr uint8_t Interesting[] = { 0, 1, 2, 7, 11, 32, 36, 38, 42, 60, 64, 72, 100, 115, 120, 126, 127 };

    uint8_t Value(Random& random)
    {
        if (random.Chance(70))
        {
            return Interesting[random.Between(0, static_cast<int>(std::size(Interesting)) - 1)];
        }

        return static_cast<uint8_t>(random.Between(0, 127));
    }

    ValueShape RandomShape(Random& random)
    {
        ValueShape shape{};

        shape.Invert = random.Chance(30);
        shape.Curve = static_cast<ValueCurve>(random.Between(0, 2));

        if (random.Chance(40))
        {
            shape.InputMinimumHundredths = random.Between(0, FullScaleHundredths);
            shape.InputMaximumHundredths = random.Between(0, FullScaleHundredths);
        }

        if (random.Chance(40))
        {
            shape.OutputMinimumHundredths = random.Between(0, FullScaleHundredths);
            shape.OutputMaximumHundredths = random.Between(0, FullScaleHundredths);
        }

        return shape;
    }

    template <typename TMap>
    void AddEntries(Random& random, TMap& map, int highest, int count)
    {
        for (int i = 0; i < count; i++)
        {
            auto const from = highest == 15 ? random.Between(0, 15) : Value(random);
            auto const to = highest == 15 ? random.Between(0, 15) : Value(random);

            map[static_cast<size_t>(from)] = static_cast<int16_t>(to);
        }
    }

    LegacyConnection RandomLegacy(Random& random)
    {
        auto legacy = MakeLegacy();

        auto& filter = legacy.Filter;

        if (random.Chance(70))
        {
            filter.IsActive = true;

            if (random.Chance(40)) for (auto& value : filter.MessageTypes) { value = random.Chance(85); }
            if (random.Chance(40)) for (auto& value : filter.Channels) { value = random.Chance(70); }
            if (random.Chance(40)) for (auto& value : filter.ChannelVoiceStatuses) { value = random.Chance(80); }
            if (random.Chance(40)) for (auto& value : filter.SystemMessages) { value = random.Chance(70); }

            if (random.Chance(40))
            {
                auto const a = Value(random);
                auto const b = Value(random);

                filter.LimitNoteRange = true;
                filter.LowestAllowedNote = (std::min)(a, b);
                filter.HighestAllowedNote = (std::max)(a, b);
            }
        }

        auto& transform = legacy.Transform;

        if (random.Chance(85))
        {
            transform.IsActive = true;
            transform.Scale = random.Chance(50) ? ValueScale::Percent : ValueScale::SevenBit;

            if (random.Chance(30)) AddEntries(random, transform.ChannelMap, 15, random.Between(1, 4));
            if (random.Chance(50)) transform.TransposeSemitones = random.Between(-24, 24);
            if (random.Chance(50)) AddEntries(random, transform.NoteMap, 127, random.Between(1, 6));

            transform.IgnoreExactPitchNotes = random.Chance(50);

            if (random.Chance(40))
            {
                transform.Curve = static_cast<VelocityCurve>(random.Between(0, 3));
                transform.FixedVelocityHundredths = random.Between(0, FullScaleHundredths);
            }

            if (random.Chance(30))
            {
                transform.RescaleVelocity = true;
                transform.MinimumVelocityHundredths = random.Between(0, FullScaleHundredths);
                transform.MaximumVelocityHundredths = random.Between(0, FullScaleHundredths);
            }

            if (random.Chance(30)) transform.AftertouchShape = RandomShape(random);
            if (random.Chance(30)) AddEntries(random, transform.ControlMap, 127, random.Between(1, 5));
            if (random.Chance(30)) AddEntries(random, transform.ProgramMap, 127, random.Between(1, 5));
            if (random.Chance(20)) AddEntries(random, transform.BankMsbMap, 127, random.Between(1, 3));
            if (random.Chance(20)) AddEntries(random, transform.BankLsbMap, 127, random.Between(1, 3));

            if (random.Chance(30))
            {
                for (int i = random.Between(1, 4); i > 0; i--)
                {
                    transform.ControlValueShapes[Value(random)] = RandomShape(random);
                }
            }
        }

        if (random.Chance(30))
        {
            legacy.SendSpeedLimit = static_cast<uint32_t>(random.Between(1, 32));
        }

        // Only what survives the version 1 writer and reader ever reaches a conversion.
        legacy.Filter = FilterFromJson(FilterToJson(legacy.Filter));
        legacy.Transform = TransformFromJson(TransformToJson(legacy.Transform));

        return legacy;
    }

    Message RandomMessage(Random& random)
    {
        auto const group = static_cast<uint8_t>(random.Between(0, 15));
        auto const channel = static_cast<uint8_t>(random.Between(0, 15));

        switch (random.Between(0, 9))
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        {
            static constexpr uint8_t statuses[] = { 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE };

            auto const data2 = random.Chance(15) ? 0 : random.Between(0, 127);

            return Midi1(group, statuses[random.Between(0, 6)], channel, Value(random), static_cast<uint8_t>(data2));
        }

        case 5:
        case 6:
        case 7:
        {
            static constexpr uint8_t statuses[] = { 0x0, 0x1, 0x2, 0x3, 0x6, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

            auto const status = statuses[random.Between(0, static_cast<int>(std::size(statuses)) - 1)];
            auto index = Value(random);
            uint8_t attribute{ 0 };
            auto data = random.Word();

            if (status == 0x8 || status == 0x9)
            {
                // An exact pitch that agrees with the note, the way a sender writes one.
                if (random.Chance(50))
                {
                    attribute = 3;
                    data = (data & 0xFFFF0000u) |
                        (static_cast<uint32_t>(index) * 512u + static_cast<uint32_t>(random.Between(0, 511)));
                }
            }
            else if (status == 0xC)
            {
                attribute = static_cast<uint8_t>(random.Between(0, 1));
                index = 0;
                data = (static_cast<uint32_t>(Value(random)) << 24) |
                    (static_cast<uint32_t>(Value(random)) << 8) |
                    static_cast<uint32_t>(Value(random));
            }

            return Midi2(group, status, channel, index, attribute, data);
        }

        case 8:
        {
            static constexpr uint8_t statuses[] = { 0xF1, 0xF2, 0xF3, 0xF4, 0xF6, 0xF8, 0xFA, 0xFB, 0xFC, 0xFE, 0xFF };

            return System(group, statuses[random.Between(0, static_cast<int>(std::size(statuses)) - 1)],
                static_cast<uint8_t>(random.Between(0, 127)), static_cast<uint8_t>(random.Between(0, 127)));
        }

        default:
            return SysEx7(group);
        }
    }

    std::wstring Describe(Message const& message)
    {
        std::wstring text{};

        for (uint8_t i = 0; i < message.Count; i++)
        {
            wchar_t buffer[12]{};
            swprintf_s(buffer, L"%08X ", message.Words[i]);
            text += buffer;
        }

        return text;
    }
}

void PatchSerializerTests::APatchSurvivesTheFile()
{
    PatchDocument patch{};
    patch.Name = L"Stage left";
    patch.Description = L"Keys to the rack";
    patch.ActivateAtStartup = false;
    patch.WaitForSendComplete = true;
    patch.CreatedTimestamp = 1790000000;
    patch.ModifiedTimestamp = 1790000500;

    auto keys = MakeEndpoint(L"keys");
    keys.CanvasX = 12.5;
    keys.CanvasY = -40;
    keys.ShowAllGroups = true;
    keys.MatchMode = EndpointMatchMode::EndpointName;
    patch.Endpoints.push_back(keys);
    patch.Endpoints.push_back(MakeEndpoint(L"rack"));

    auto split = MakeBlock(L"split", BlockKind::NoteFilter);
    split.Name = L"Lower half";
    split.CanvasX = 300;
    split.CanvasY = 120;
    split.Settings.Values.Highest = 59;
    patch.Blocks.push_back(split);

    auto up = MakeBlock(L"up", BlockKind::Transpose);
    up.Bypassed = true;
    up.Settings.Transform.TransposeSemitones = 12;
    patch.Blocks.push_back(up);

    auto slow = MakeBlock(L"slow", BlockKind::Throttle);
    slow.Settings.SendSpeedLimit = 3;
    patch.Blocks.push_back(slow);

    auto first = MakeLink(L"l1", L"keys", L"split");
    first.SourceGroupIndex = 4;
    patch.Connections.push_back(first);

    patch.Connections.push_back(MakeLink(L"l2", L"split", L"up"));
    patch.Connections.push_back(MakeLink(L"l3", L"up", L"slow"));

    auto last = MakeLink(L"l4", L"slow", L"rack");
    last.DestinationGroupIndex = 9;
    last.Muted = true;
    patch.Connections.push_back(last);

    auto direct = MakeLink(L"l5", L"keys", L"rack");
    patch.Connections.push_back(direct);

    auto const text = WritePatchJson(patch);
    VERIFY_IS_FALSE(text.empty());

    auto const back = ReadPatchJson(text, L"fallback");
    VERIFY_IS_TRUE(back.has_value());

    VERIFY_ARE_EQUAL(CurrentPatchFileVersion, back->LoadedFileVersion);
    VERIFY_ARE_EQUAL(patch.Name, back->Name);
    VERIFY_ARE_EQUAL(patch.Description, back->Description);
    VERIFY_IS_FALSE(back->ActivateAtStartup);
    VERIFY_IS_TRUE(back->WaitForSendComplete);
    VERIFY_ARE_EQUAL(patch.CreatedTimestamp, back->CreatedTimestamp);
    VERIFY_ARE_EQUAL(patch.ModifiedTimestamp, back->ModifiedTimestamp);

    VERIFY_ARE_EQUAL(size_t{ 2 }, back->Endpoints.size());
    VERIFY_ARE_EQUAL(12.5, back->Endpoints[0].CanvasX);
    VERIFY_ARE_EQUAL(-40.0, back->Endpoints[0].CanvasY);
    VERIFY_IS_TRUE(back->Endpoints[0].ShowAllGroups);
    VERIFY_IS_TRUE(back->Endpoints[0].MatchMode == EndpointMatchMode::EndpointName);

    VERIFY_ARE_EQUAL(patch.Blocks.size(), back->Blocks.size());

    for (size_t i = 0; i < patch.Blocks.size(); i++)
    {
        auto const& before = patch.Blocks[i];
        auto const& after = back->Blocks[i];

        VERIFY_ARE_EQUAL(before.Id, after.Id);
        VERIFY_IS_TRUE(before.Kind == after.Kind);
        VERIFY_ARE_EQUAL(before.Name, after.Name);
        VERIFY_ARE_EQUAL(before.CanvasX, after.CanvasX);
        VERIFY_ARE_EQUAL(before.CanvasY, after.CanvasY);
        VERIFY_ARE_EQUAL(before.Bypassed, after.Bypassed);
        VERIFY_ARE_EQUAL(
            BlockSettingsSignature(before.Kind, before.Settings),
            BlockSettingsSignature(after.Kind, after.Settings));
    }

    VERIFY_ARE_EQUAL(patch.Connections.size(), back->Connections.size());

    for (size_t i = 0; i < patch.Connections.size(); i++)
    {
        auto const& before = patch.Connections[i];
        auto const& after = back->Connections[i];

        VERIFY_ARE_EQUAL(before.Id, after.Id);
        VERIFY_ARE_EQUAL(before.SourceId, after.SourceId);
        VERIFY_ARE_EQUAL(before.SourceGroupIndex, after.SourceGroupIndex);
        VERIFY_ARE_EQUAL(before.DestinationId, after.DestinationId);
        VERIFY_ARE_EQUAL(before.DestinationGroupIndex, after.DestinationGroupIndex);
        VERIFY_ARE_EQUAL(before.Muted, after.Muted);
    }

    // A block the customer never named is written without a name, so it reads in whatever
    // language opens the file.
    VERIFY_IS_TRUE(text.find(L"\"Lower half\"") != std::wstring::npos);
    VERIFY_ARE_EQUAL(std::wstring{}, back->Blocks[1].Name);
}

void PatchSerializerTests::OnlyEndpointEndsHaveAGroup()
{
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "a", "match": {} }, { "id": "b", "match": {} } ],
        "blocks": [ { "id": "f", "type": "channelFilter", "x": 0, "y": 0, "settings": { "channels": 1 } } ],
        "connections": [
            { "id": "1", "source": "a", "sourceGroup": 3, "destination": "f", "destinationGroup": 5 },
            { "id": "2", "source": "f", "sourceGroup": 7, "destination": "b", "destinationGroup": 99 }
        ]
    })", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 2 }, patch->Connections.size());

    VERIFY_ARE_EQUAL(3, patch->Connections[0].SourceGroupIndex);
    VERIFY_ARE_EQUAL(AllGroups, patch->Connections[0].DestinationGroupIndex);
    VERIFY_ARE_EQUAL(AllGroups, patch->Connections[1].SourceGroupIndex);

    // Out of range means all groups, as it always has.
    VERIFY_ARE_EQUAL(AllGroups, patch->Connections[1].DestinationGroupIndex);

    auto const text = WritePatchJson(patch.value());
    auto const root = json::JsonObject::Parse(text);
    auto const links = root.GetNamedArray(L"connections");

    VERIFY_IS_TRUE(links.GetObjectAt(0).HasKey(L"sourceGroup"));
    VERIFY_IS_FALSE(links.GetObjectAt(0).HasKey(L"destinationGroup"));
    VERIFY_IS_FALSE(links.GetObjectAt(1).HasKey(L"sourceGroup"));
    VERIFY_IS_TRUE(links.GetObjectAt(1).HasKey(L"destinationGroup"));
    VERIFY_ARE_EQUAL(2.0, root.GetNamedNumber(L"fileVersion"));
}

void PatchSerializerTests::AnEarlierFileBecomesBlocks()
{
    auto const patch = ReadPatchJson(Version1Patch, L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(1, patch->LoadedFileVersion);
    VERIFY_ARE_EQUAL(std::wstring{ L"Old patch" }, patch->Name);
    VERIFY_IS_TRUE(patch->ConversionIssues.empty());

    auto const kinds = ChainKinds(patch.value(), L"kb");

    VERIFY_ARE_EQUAL(size_t{ 4 }, kinds.size());
    VERIFY_IS_TRUE(kinds[0] == BlockKind::MessageTypeFilter);
    VERIFY_IS_TRUE(kinds[1] == BlockKind::ChannelFilter);
    VERIFY_IS_TRUE(kinds[2] == BlockKind::Transpose);
    VERIFY_IS_TRUE(kinds[3] == BlockKind::Throttle);

    VERIFY_ARE_EQUAL(size_t{ 4 }, patch->Blocks.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, patch->Connections.size());

    // The first link keeps the connection's id and its source group; the last keeps its
    // destination group.
    auto const* first = patch->FindConnection(L"c1");
    VERIFY_IS_NOT_NULL(first);
    VERIFY_ARE_EQUAL(std::wstring{ L"kb" }, first->SourceId);
    VERIFY_ARE_EQUAL(AllGroups, first->SourceGroupIndex);

    auto const last = std::find_if(patch->Connections.begin(), patch->Connections.end(),
        [](PatchConnection const& c) { return c.DestinationId == L"syn"; });

    VERIFY_IS_TRUE(last != patch->Connections.end());
    VERIFY_ARE_EQUAL(2, last->DestinationGroupIndex);

    // Clock is kept out, channel 3 is kept out, and notes go up an octave.
    std::wstring destination{};

    auto clock = System(0, 0xF8);
    VERIFY_IS_FALSE(RunConvertedChain(patch.value(), L"kb", clock, destination));

    auto start = System(0, 0xFA);
    VERIFY_IS_TRUE(RunConvertedChain(patch.value(), L"kb", start, destination));

    auto third = Midi1(0, NoteOn, 2, 60, 100);
    VERIFY_IS_FALSE(RunConvertedChain(patch.value(), L"kb", third, destination));

    auto note = Midi1(0, NoteOn, 1, 60, 100);
    VERIFY_IS_TRUE(RunConvertedChain(patch.value(), L"kb", note, destination));
    VERIFY_IS_TRUE(note == Midi1(0, NoteOn, 1, 72, 100));
    VERIFY_ARE_EQUAL(std::wstring{ L"syn" }, destination);

    auto const* throttle = std::find_if(patch->Blocks.begin(), patch->Blocks.end(),
        [](PatchBlock const& b) { return b.Kind == BlockKind::Throttle; }).operator->();
    VERIFY_ARE_EQUAL(2u, throttle->Settings.SendSpeedLimit);

    // Laid out left to right in the order messages flow.
    double previousX = patch->FindEndpoint(L"kb")->CanvasX;

    for (auto const& block : patch->Blocks)
    {
        VERIFY_IS_TRUE(block.CanvasX > previousX);
        previousX = block.CanvasX;
    }

    VERIFY_IS_TRUE(patch->FindEndpoint(L"syn")->CanvasX > previousX);

    // Written again, it is a version 2 file that reads back the same.
    auto const again = ReadPatchJson(WritePatchJson(patch.value()), L"fallback");

    VERIFY_IS_TRUE(again.has_value());
    VERIFY_ARE_EQUAL(CurrentPatchFileVersion, again->LoadedFileVersion);
    VERIFY_ARE_EQUAL(patch->Blocks.size(), again->Blocks.size());
    VERIFY_ARE_EQUAL(patch->Connections.size(), again->Connections.size());
}

void PatchSerializerTests::APlainEarlierConnectionStaysPlain()
{
    auto const patch = ReadPatchJson(PlainVersion1Patch, L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_IS_TRUE(patch->Blocks.empty());

    // The second connection repeats the first, and version 1 showed only the first.
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Connections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"c1" }, patch->Connections[0].Id);
    VERIFY_ARE_EQUAL(0, patch->Connections[0].SourceGroupIndex);
    VERIFY_ARE_EQUAL(AllGroups, patch->Connections[0].DestinationGroupIndex);
    VERIFY_IS_TRUE(patch->Connections[0].Muted);

    // Nothing was added, so the canvas stays the way the customer left it.
    VERIFY_ARE_EQUAL(460.0, patch->FindEndpoint(L"syn")->CanvasX);
    VERIFY_ARE_EQUAL(200.0, patch->FindEndpoint(L"syn")->CanvasY);
}

void PatchSerializerTests::ConvertedConnectionsDoExactlyWhatTheyDid()
{
    Random random{};

    size_t compared{ 0 };
    size_t converted{ 0 };
    size_t mismatches{ 0 };
    std::wstring firstMismatch{};

    for (int round = 0; round < 600; round++)
    {
        auto const legacy = RandomLegacy(random);
        auto patch = TwoEndpoints();

        VERIFY_IS_TRUE(ConvertLegacyConnection(patch, legacy));

        // Those are covered on their own below.
        if (!patch.ConversionIssues.empty())
        {
            continue;
        }

        converted++;

        for (int i = 0; i < 300; i++)
        {
            auto const message = RandomMessage(random);

            auto before = message;
            auto const versionOne = RunVersion1(legacy, before);

            auto after = message;
            std::wstring destination{};
            auto const blocks = RunConvertedChain(patch, L"in", after, destination);

            compared++;

            auto const same = versionOne == blocks &&
                (!versionOne || (before == after && destination == L"out"));

            if (!same && mismatches++ == 0)
            {
                firstMismatch = L"round " + std::to_wstring(round) + L": " + Describe(message) +
                    L"version 1 " + (versionOne ? Describe(before) : std::wstring{ L"kept out " }) +
                    L"blocks " + (blocks ? Describe(after) : std::wstring{ L"kept out" });
            }
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, mismatches, firstMismatch.c_str());
    VERIFY_IS_TRUE(converted > 500);
    VERIFY_IS_TRUE(compared > 150000);
}

void PatchSerializerTests::ANoteMapNextToATransposeKeepsItsTargets()
{
    std::wstring destination{};

    {
        // Map first: the drum pad lands where it was mapped, and everything else goes up.
        auto legacy = MakeLegacy();
        legacy.Transform.IsActive = true;
        legacy.Transform.TransposeSemitones = 12;
        legacy.Transform.NoteMap[36] = 38;

        auto patch = TwoEndpoints();
        VERIFY_IS_TRUE(ConvertLegacyConnection(patch, legacy));
        VERIFY_IS_TRUE(patch.ConversionIssues.empty());

        auto const kinds = ChainKinds(patch, L"in");
        VERIFY_ARE_EQUAL(size_t{ 2 }, kinds.size());
        VERIFY_IS_TRUE(kinds[0] == BlockKind::NoteMap);
        VERIFY_IS_TRUE(kinds[1] == BlockKind::Transpose);

        auto mapped = Midi1(0, NoteOn, 9, 36, 100);
        VERIFY_IS_TRUE(RunConvertedChain(patch, L"in", mapped, destination));
        VERIFY_IS_TRUE(mapped == Midi1(0, NoteOn, 9, 38, 100));

        auto moved = Midi1(0, NoteOn, 9, 40, 100);
        VERIFY_IS_TRUE(RunConvertedChain(patch, L"in", moved, destination));
        VERIFY_IS_TRUE(moved == Midi1(0, NoteOn, 9, 52, 100));
    }

    {
        // A target the transpose would push off the bottom: the transpose goes first instead.
        auto legacy = MakeLegacy();
        legacy.Transform.IsActive = true;
        legacy.Transform.TransposeSemitones = 12;
        legacy.Transform.NoteMap[60] = 5;

        auto patch = TwoEndpoints();
        VERIFY_IS_TRUE(ConvertLegacyConnection(patch, legacy));
        VERIFY_IS_TRUE(patch.ConversionIssues.empty());

        auto const kinds = ChainKinds(patch, L"in");
        VERIFY_ARE_EQUAL(size_t{ 2 }, kinds.size());
        VERIFY_IS_TRUE(kinds[0] == BlockKind::Transpose);
        VERIFY_IS_TRUE(kinds[1] == BlockKind::NoteMap);

        auto mapped = Midi1(0, NoteOn, 0, 60, 100);
        VERIFY_IS_TRUE(RunConvertedChain(patch, L"in", mapped, destination));
        VERIFY_IS_TRUE(mapped == Midi1(0, NoteOn, 0, 5, 100));

        auto moved = Midi1(0, NoteOn, 0, 61, 100);
        VERIFY_IS_TRUE(RunConvertedChain(patch, L"in", moved, destination));
        VERIFY_IS_TRUE(moved == Midi1(0, NoteOn, 0, 73, 100));
    }
}

void PatchSerializerTests::ANoteMapEntryThatCannotBeKeptIsReported()
{
    // Note 120 to note 5, next to an octave up: neither order can keep it.
    auto legacy = MakeLegacy();
    legacy.Transform.IsActive = true;
    legacy.Transform.TransposeSemitones = 12;
    legacy.Transform.NoteMap[120] = 5;
    legacy.Transform.NoteMap[36] = 38;

    auto patch = TwoEndpoints();
    VERIFY_IS_TRUE(ConvertLegacyConnection(patch, legacy));

    VERIFY_ARE_EQUAL(size_t{ 1 }, patch.ConversionIssues.size());

    auto const& issue = patch.ConversionIssues[0];
    VERIFY_IS_TRUE(issue.Kind == ConversionIssueKind::NoteMapEntryOutOfRange);
    VERIFY_ARE_EQUAL(120, static_cast<int>(issue.FromNote));
    VERIFY_ARE_EQUAL(5, static_cast<int>(issue.ToNote));

    auto const* block = patch.FindBlock(issue.BlockId);
    VERIFY_IS_NOT_NULL(block);
    VERIFY_IS_TRUE(block->Kind == BlockKind::NoteMap);

    // The entries that could be kept still are.
    std::wstring destination{};

    auto kept = Midi1(0, NoteOn, 0, 36, 100);
    VERIFY_IS_TRUE(RunConvertedChain(patch, L"in", kept, destination));
    VERIFY_IS_TRUE(kept == Midi1(0, NoteOn, 0, 38, 100));
}

void PatchSerializerTests::AMutedConnectionStaysMuted()
{
    auto legacy = MakeLegacy();
    legacy.Muted = true;
    legacy.Transform.IsActive = true;
    legacy.Transform.TransposeSemitones = -5;
    legacy.SendSpeedLimit = 1;

    auto patch = TwoEndpoints();
    VERIFY_IS_TRUE(ConvertLegacyConnection(patch, legacy));

    VERIFY_ARE_EQUAL(size_t{ 3 }, patch.Connections.size());

    for (auto const& link : patch.Connections)
    {
        // Muting the way in is enough: nothing reaches the rest.
        VERIFY_ARE_EQUAL(link.SourceId == L"in", link.Muted);
    }
}

void PatchSerializerTests::LinksToMissingThingsAreLeftOut()
{
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "a", "match": {}, "x": 1e9, "y": -1e9 }, { "id": "b", "match": {} }, { "id": "a", "match": {} } ],
        "blocks": [
            { "id": "f", "type": "noteFilter", "x": 0, "y": 0 },
            { "id": "f", "type": "transpose", "x": 0, "y": 0 },
            { "id": "a", "type": "transpose", "x": 0, "y": 0 },
            { "type": "transpose" },
            "nonsense"
        ],
        "connections": [
            { "id": "1", "source": "a", "destination": "f" },
            { "id": "2", "source": "f", "destination": "ghost" },
            { "id": "3", "source": "f", "destination": "f" },
            { "id": "4", "source": "a", "destination": "f" },
            { "id": "1", "source": "f", "destination": "b" },
            { "source": "a", "sourceGroup": 2, "destination": "b" },
            42
        ]
    })", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());

    VERIFY_ARE_EQUAL(size_t{ 2 }, patch->Endpoints.size());
    VERIFY_ARE_EQUAL(100000.0, patch->Endpoints[0].CanvasX);
    VERIFY_ARE_EQUAL(-100000.0, patch->Endpoints[0].CanvasY);

    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Blocks.size());
    VERIFY_IS_TRUE(patch->Blocks[0].Kind == BlockKind::NoteFilter);

    // a to f, f to b under a fresh id, and a to b.
    VERIFY_ARE_EQUAL(size_t{ 3 }, patch->Connections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"1" }, patch->Connections[0].Id);
    VERIFY_ARE_NOT_EQUAL(std::wstring{ L"1" }, patch->Connections[1].Id);
    VERIFY_IS_FALSE(patch->Connections[1].Id.empty());
    VERIFY_IS_FALSE(patch->Connections[2].Id.empty());
    VERIFY_ARE_EQUAL(2, patch->Connections[2].SourceGroupIndex);

    // The name comes from the file name when the file has none.
    VERIFY_ARE_EQUAL(std::wstring{ L"fallback" }, patch->Name);
}

void PatchSerializerTests::AnUnknownBlockIsLeftOutWithItsLinks()
{
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 3,
        "endpoints": [ { "id": "a", "match": {} }, { "id": "b", "match": {} } ],
        "blocks": [
            { "id": "x", "type": "somethingFromLater", "x": 0, "y": 0 },
            { "id": "t", "type": "transpose", "x": 0, "y": 0, "settings": { "transposeSemitones": 3 } }
        ],
        "connections": [
            { "id": "1", "source": "a", "destination": "x" },
            { "id": "2", "source": "x", "destination": "t" },
            { "id": "3", "source": "t", "destination": "b" }
        ]
    })", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(3, patch->LoadedFileVersion);
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Blocks.size());
    VERIFY_ARE_EQUAL(3, patch->Blocks[0].Settings.Transform.TransposeSemitones);
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Connections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"3" }, patch->Connections[0].Id);
}

void PatchSerializerTests::TextThatIsNotAPatchIsRejected()
{
    VERIFY_IS_FALSE(ReadPatchJson(L"", L"fallback").has_value());
    VERIFY_IS_FALSE(ReadPatchJson(L"not a patch", L"fallback").has_value());
    VERIFY_IS_FALSE(ReadPatchJson(L"[1, 2, 3]", L"fallback").has_value());
    VERIFY_IS_FALSE(ReadPatchJson(L"{ \"name\": ", L"fallback").has_value());

    auto const empty = ReadPatchJson(L"{}", L"Named by its file");
    VERIFY_IS_TRUE(empty.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Named by its file" }, empty->Name);
    VERIFY_IS_TRUE(empty->Endpoints.empty());
    VERIFY_IS_TRUE(empty->Connections.empty());
}

void PatchSerializerTests::RemovingABlockRemovesItsLinks()
{
    auto patch = TwoEndpoints();
    patch.Blocks.push_back(MakeBlock(L"t", BlockKind::Transpose));
    patch.Connections.push_back(MakeLink(L"1", L"in", L"t"));
    patch.Connections.push_back(MakeLink(L"2", L"t", L"out"));
    patch.Connections.push_back(MakeLink(L"3", L"in", L"out"));

    VERIFY_IS_TRUE(patch.IsBlock(L"t"));
    VERIFY_IS_FALSE(patch.IsBlock(L"in"));
    VERIFY_IS_TRUE(patch.HasNode(L"in"));

    patch.RemoveBlock(L"t");

    VERIFY_IS_TRUE(patch.Blocks.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch.Connections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"3" }, patch.Connections[0].Id);

    patch.RemoveEndpoint(L"out");

    VERIFY_IS_TRUE(patch.Connections.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch.Endpoints.size());

    // Ids are unique and carry no braces.
    auto const one = PatchDocument::NewId();
    auto const two = PatchDocument::NewId();

    VERIFY_ARE_NOT_EQUAL(one, two);
    VERIFY_ARE_EQUAL(std::wstring::npos, one.find(L'{'));
}
