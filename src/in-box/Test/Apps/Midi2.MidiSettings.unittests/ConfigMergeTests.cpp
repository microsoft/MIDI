// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// UNSUPPORTED - DO NOT COPY THIS APPROACH. These tests drive code that reads and writes the
// Windows MIDI Services configuration file directly, in a temporary folder of their own.
// Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.

#include <windows.h>

#include <algorithm>
#include <filesystem>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include "ConfigEngineTestSupport.h"
#include "ConfigMergeTests.h"

#include "ConfigBackups.h"
#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ImportApply.h"
#include "ImportPlan.h"

using namespace WEX::Logging;
using namespace configtests;

namespace
{
    std::wstring const Ks{ L"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}" };
    std::wstring const Loop{ L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}" };
    std::wstring const Network{ L"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}" };
    std::wstring const Rtp{ L"{54C9B2F6-C235-4000-A675-9F6958A1A4FA}" };
    std::wstring const Bluetooth{ L"{5DC87270-F318-4838-A4F9-6AADC63E925F}" };
    std::wstring const Synth{ L"{7605713E-FEA9-409D-A90F-A81233200D0A}" };
    std::wstring const Unknown{ L"{99999999-0000-0000-0000-000000000001}" };

    // this PC, STAGE-LAPTOP, and the configuration it uses, "Live set"
    std::wstring const HereSettings{ LR"json({
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}": { "update": [
            { "match": { "endpointDeviceId": "dev-keystep" }, "customProperties": { "name": "KeyStep", "image": "ep-keystep.png" } },
            { "match": { "endpointDeviceId": "dev-umc" }, "customProperties": { "name": "Interface" } },
            { "match": { "endpointDeviceId": "dev-piano" }, "customProperties": { "name": "Piano", "image": "ep-piano.png" } }
        ] },
        "{942BF02D-93C0-4EA8-B03E-D51156CA75E1}": { "create": {
            "{DF88CA43-94B3-4795-AAAE-7CC8396B9A06}": { "endpointA": { "name": "Glass to DAW (A)", "uniqueIdentifier": "GLASS" }, "endpointB": { "name": "Glass to DAW (B)", "uniqueIdentifier": "GLASS" } },
            "{22222222-0000-0000-0000-000000000001}": { "endpointA": { "name": "Max (A)", "uniqueIdentifier": "MAXHERE" }, "endpointB": { "name": "Max (B)", "uniqueIdentifier": "MAXHERE" } }
        } },
        "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": {
            "create": {
                "hosts": { "{bbbbbbbb-0000-0000-0000-000000000001}": { "name": "Practice", "serviceInstanceName": "Practice", "productInstanceId": "winmidisrvBBBBBBBB-0000-0000-0000-00000000" } },
                "clients": { "{cccccccc-0000-0000-0000-000000000001}": { "match": { "umpEndpointName": "Mac Studio", "id": "DnsSd#Mac Studio._midi2._udp.local#0" } } }
            },
            "transportSettings": { "outboundPingInterval": 1000 }
        },
        "{5DC87270-F318-4838-A4F9-6AADC63E925F}": { "devices": [ { "deviceId": "AA0000000001", "name": "LUMI" } ], "peripheral": { "enabled": true, "advertisedName": "STAGE-LAPTOP" } },
        "{7605713E-FEA9-409D-A90F-A81233200D0A}": { "synthMode": "compatible", "audioMode": "sharedLowLatency" }
    })json" };

    // a package from STUDIO-PC, "Studio"
    std::wstring const PackageSettings{ LR"json({
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}": { "update": [
            { "match": { "endpointDeviceId": "DEV-KEYSTEP" }, "customProperties": { "name": "KeyStep (Studio)", "image": "ep-keystep.png" } },
            { "match": { "endpointDeviceId": "dev-matriarch" }, "customProperties": { "name": "Matriarch (Studio A)", "image": "ep-matriarch.png" } },
            { "match": { "endpointDeviceId": "dev-digitakt" }, "customProperties": { "name": "Digitakt" } }
        ] },
        "{942BF02D-93C0-4EA8-B03E-D51156CA75E1}": { "create": {
            "{DF88CA43-94B3-4795-AAAE-7CC8396B9A06}": { "endpointA": { "name": "Glass to DAW (A)", "uniqueIdentifier": "GLASS" }, "endpointB": { "name": "Glass to DAW (B)", "uniqueIdentifier": "GLASS" } },
            "{22222222-0000-0000-0000-000000000001}": { "endpointA": { "name": "DAW Out (A)", "uniqueIdentifier": "MAXHERE" }, "endpointB": { "name": "DAW Out (B)", "uniqueIdentifier": "MAXHERE" } },
            "{33333333-0000-0000-0000-000000000001}": { "endpointA": { "name": "Max (A)", "uniqueIdentifier": "MAXSTUDIO" }, "endpointB": { "name": "Max (B)", "uniqueIdentifier": "MAXSTUDIO" } }
        } },
        "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": {
            "create": {
                "hosts": {
                    "{aaaaaaaa-0000-0000-0000-000000000001}": { "name": "STUDIO-PC", "serviceInstanceName": "STUDIO-PC_windows_midisrv", "productInstanceId": "winmidisrvAAAAAAAA-0000-0000-0000-00000000", "networkAdapterId": "{12345678-1111-2222-3333-444444444444}" },
                    "{bbbbbbbb-0000-0000-0000-000000000002}": { "name": "Practice", "serviceInstanceName": "Practice" }
                },
                "clients": {
                    "{cccccccc-0000-0000-0000-000000000001}": { "match": { "umpEndpointName": "BomeBox", "id": "DnsSd#bomebox#0" } },
                    "{cccccccc-0000-0000-0000-000000000002}": { "match": { "umpEndpointName": "Mac Studio", "id": "DnsSd#Mac Studio._midi2._udp.local#0" } }
                }
            },
            "updateEntries": { "clients": { "{cccccccc-0000-0000-0000-000000000001}": { "sendSpeedLimit": 4 } } },
            "transportSettings": { "outboundPingInterval": 5000 }
        },
        "{54C9B2F6-C235-4000-A675-9F6958A1A4FA}": { "create": { "hosts": { "{DDDDDDDD-0000-0000-0000-000000000001}": { "port": "auto" } } } },
        "{5DC87270-F318-4838-A4F9-6AADC63E925F}": { "devices": [ { "deviceId": "AA0000000001", "name": "LUMI" }, { "deviceId": "AA0000000002", "name": "Chorda" } ], "peripheral": { "enabled": false, "advertisedName": "STUDIO-PC" } },
        "{7605713E-FEA9-409D-A90F-A81233200D0A}": { "synthMode": "modern", "audioMode": "exclusive" },
        "{99999999-0000-0000-0000-000000000001}": { "x": 1 }
    })json" };

    std::wstring const HereFileName{ L"Live set.midiconfig.json" };

    struct Fixture
    {
        TestFolder Folder{};
        std::shared_ptr<FakeSystem> System{ std::make_shared<FakeSystem>() };
        engine::ConfigEnvironment Environment{};
        engine::PackageContents Contents{};
        engine::ImportPlan Plan{};
        std::string HereBytes{};

        explicit Fixture(std::wstring const& hereSettings = HereSettings, bool const rtpInstalled = true)
        {
            Environment = MakeEnvironment(Folder, System);

            HereBytes = MakeConfig(L"Live set", hereSettings);
            WriteFileBytes(Folder.Config() + HereFileName, HereBytes);
            System->CurrentFileName = HereFileName;

            WriteFileBytes(Folder.Pictures() + L"ep-keystep.png", PictureBytes(1));
            WriteFileBytes(Folder.Pictures() + L"ep-piano.png", PictureBytes(2));

            Contents.Info.ConfigName = L"Studio";
            Contents.Info.ComputerName = L"STUDIO-PC";
            Contents.Info.Note = L"Everything from the studio desktop";
            Contents.Info.Parts = engine::PartBit(engine::ConfigPart::All);
            Contents.ConfigurationBytes = MakeConfig(L"Studio", PackageSettings);

            auto const keystep = PictureBytes(10);
            auto const matriarch = PictureBytes(11);

            Contents.Pictures.emplace_back(L"ep-keystep.png", std::vector<uint8_t>{ keystep.begin(), keystep.end() });
            Contents.Pictures.emplace_back(L"ep-matriarch.png", std::vector<uint8_t>{ matriarch.begin(), matriarch.end() });

            VERIFY_IS_TRUE(engine::MakeImportPlan(Environment, Contents, rtpInstalled, Plan).Succeeded());
            VERIFY_IS_TRUE(Plan.HasTarget);
        }

        json::JsonObject Merge(engine::ImportChoices const& choices, std::vector<engine::ResultLine>& lines) const
        {
            auto const merged = engine::MergeImport(Plan, Contents, choices, {}, lines);
            VERIFY_IS_NOT_NULL(merged);

            return merged;
        }

        json::JsonObject Merge(engine::ImportChoices const& choices) const
        {
            std::vector<engine::ResultLine> lines{};

            return Merge(choices, lines);
        }

        engine::PlanItem const& Item(std::wstring const& id) const
        {
            for (auto const& item : Plan.Items)
            {
                if (item.Id == id)
                {
                    return item;
                }
            }

            VERIFY_FAIL(L"No such plan item.");
            return Plan.Items.front();
        }
    };

    std::wstring CustomName(json::JsonObject const& section, std::wstring const& endpointDeviceId)
    {
        auto const update = engine::jsonutil::ArrayValue(section, L"update");

        for (auto const& value : update)
        {
            auto const entry = value.GetObject();
            auto const match = engine::jsonutil::ObjectValue(entry, L"match");

            if (engine::SameText(engine::jsonutil::StringValue(match, L"endpointDeviceId"), endpointDeviceId))
            {
                return engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(entry, L"customProperties"), L"name");
            }
        }

        return {};
    }

    uint32_t CountOf(json::JsonObject const& section, std::wstring const& key)
    {
        auto const array = engine::jsonutil::ArrayValue(section, key);

        return array == nullptr ? 0 : array.Size();
    }

    json::JsonObject CreateList(json::JsonObject const& section, std::wstring const& list)
    {
        return engine::jsonutil::ObjectValue(engine::jsonutil::ObjectValue(section, L"create"), list);
    }

    // the one entry in an object keyed by ID whose name, at the given path, is the given name
    json::JsonObject EntryNamed(json::JsonObject const& entries, std::wstring const& name, std::wstring& key)
    {
        for (auto const& pair : entries)
        {
            auto const entry = pair.Value().GetObject();

            if (engine::jsonutil::StringValue(entry, L"name") == name ||
                engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(entry, L"endpointA"), L"name") == name ||
                engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(entry, L"match"), L"umpEndpointName") == name)
            {
                key = pair.Key();
                return entry;
            }
        }

        key.clear();
        return nullptr;
    }

    bool HasLine(std::vector<engine::ResultLine> const& lines, engine::ResultCode const code)
    {
        return std::any_of(lines.begin(), lines.end(), [code](auto const& line) { return line.Code == code; });
    }

    engine::ResultLine const& LineFor(std::vector<engine::ResultLine> const& lines, engine::ResultCode const code)
    {
        for (auto const& line : lines)
        {
            if (line.Code == code)
            {
                return line;
            }
        }

        VERIFY_FAIL(L"No such result line.");
        return lines.front();
    }
}

void ConfigMergeTests::TestDefaultsKeepYourNames()
{
    Fixture fixture{};

    VERIFY_IS_TRUE(fixture.Item(L"device:0").Status == engine::ItemStatus::InBoth);
    VERIFY_IS_TRUE(fixture.Item(L"device:0").YourName == L"KeyStep");
    VERIFY_IS_TRUE(engine::OutcomeFor(fixture.Item(L"device:0"), {}) == engine::ItemOutcome::KeepsYourName);
    VERIFY_IS_TRUE(engine::OutcomeFor(fixture.Item(L"device:1"), {}) == engine::ItemOutcome::WillBeAdded);

    std::vector<engine::ResultLine> lines{};
    auto const merged = fixture.Merge({}, lines);
    auto const ks = Section(merged, Ks);

    VERIFY_ARE_EQUAL(CountOf(ks, L"update"), (uint32_t)5);
    VERIFY_IS_TRUE(CustomName(ks, L"dev-keystep") == L"KeyStep");
    VERIFY_IS_TRUE(CustomName(ks, L"dev-umc") == L"Interface");
    VERIFY_IS_TRUE(CustomName(ks, L"dev-matriarch") == L"Matriarch (Studio A)");
    VERIFY_IS_TRUE(CustomName(ks, L"dev-digitakt") == L"Digitakt");

    auto const& line = LineFor(lines, engine::ResultCode::DeviceNames);
    VERIFY_ARE_EQUAL(line.Count, (uint32_t)2);
    VERIFY_ARE_EQUAL(line.Count3, (uint32_t)1);
}

void ConfigMergeTests::TestUseThePackagesNamesReplacesShared()
{
    Fixture fixture{};

    engine::ImportChoices choices{};
    choices.SharedDevices = engine::SharedDevicesChoice::UseThePackages;

    auto const ks = Section(fixture.Merge(choices), Ks);

    VERIFY_ARE_EQUAL(CountOf(ks, L"update"), (uint32_t)5);
    VERIFY_IS_TRUE(CustomName(ks, L"dev-keystep") == L"KeyStep (Studio)");
    VERIFY_IS_TRUE(CustomName(ks, L"dev-piano") == L"Piano");
}

void ConfigMergeTests::TestReplaceMineListsAndRemovesYours()
{
    Fixture fixture{};

    engine::ImportChoices choices{};
    choices.DeviceNames = engine::DeviceNamesChoice::ReplaceMine;

    // the review lists exactly what goes
    uint32_t losing{ 0 };

    for (auto const& item : fixture.Plan.Items)
    {
        if (engine::OutcomeFor(item, choices) == engine::ItemOutcome::LosesYourName)
        {
            losing++;
            VERIFY_IS_TRUE(item.YourName == L"Interface" || (item.YourName == L"Piano" && item.YourImage == L"ep-piano.png"));
        }
    }

    VERIFY_ARE_EQUAL(losing, (uint32_t)2);
    VERIFY_IS_TRUE(engine::OutcomeFor(fixture.Item(L"device:0"), choices) == engine::ItemOutcome::GetsThePackagesName);

    std::vector<engine::ResultLine> lines{};
    auto const ks = Section(fixture.Merge(choices, lines), Ks);

    VERIFY_ARE_EQUAL(CountOf(ks, L"update"), (uint32_t)3);
    VERIFY_IS_TRUE(CustomName(ks, L"dev-umc").empty());
    VERIFY_IS_TRUE(CustomName(ks, L"dev-keystep") == L"KeyStep (Studio)");

    auto const& line = LineFor(lines, engine::ResultCode::DeviceNames);
    VERIFY_ARE_EQUAL(line.Count, (uint32_t)2);
    VERIFY_ARE_EQUAL(line.Count4, (uint32_t)1);
    VERIFY_ARE_EQUAL(line.Count5, (uint32_t)2);
}

void ConfigMergeTests::TestDontImportLeavesDeviceNamesAlone()
{
    Fixture fixture{};

    engine::ImportChoices choices{};
    choices.DeviceNames = engine::DeviceNamesChoice::DontImport;

    std::vector<engine::ResultLine> lines{};
    auto const ks = Section(fixture.Merge(choices, lines), Ks);

    VERIFY_ARE_EQUAL(CountOf(ks, L"update"), (uint32_t)3);
    VERIFY_IS_TRUE(CustomName(ks, L"dev-keystep") == L"KeyStep");
    VERIFY_IS_TRUE(HasLine(lines, engine::ResultCode::DeviceNamesNotImported));
    VERIFY_IS_TRUE(engine::PicturesToImport(fixture.Plan, choices).empty());
}

void ConfigMergeTests::TestPicturesAreStoredAndRenamed()
{
    Fixture fixture{};

    engine::ImportChoices choices{};
    choices.SharedDevices = engine::SharedDevicesChoice::UseThePackages;

    auto const result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, choices);

    VERIFY_IS_TRUE(result.Succeeded());

    auto const written = Parse(ReadFileBytes(fixture.Folder.Config() + HereFileName));
    auto const ks = Section(written, Ks);

    // a different picture with the same name is kept apart, and only the imported entry points at it
    auto const update = engine::jsonutil::ArrayValue(ks, L"update");

    bool sawPackageKeyStep{ false };
    bool sawPiano{ false };

    for (auto const& value : update)
    {
        auto const properties = engine::jsonutil::ObjectValue(value.GetObject(), L"customProperties");
        auto const name = engine::jsonutil::StringValue(properties, L"name");
        auto const image = engine::jsonutil::StringValue(properties, L"image");

        if (name == L"KeyStep (Studio)")
        {
            VERIFY_IS_TRUE(image == L"ep-keystep (1).png");
            sawPackageKeyStep = true;
        }
        else if (name == L"Piano")
        {
            VERIFY_IS_TRUE(image == L"ep-piano.png");
            sawPiano = true;
        }
        else if (name == L"Matriarch (Studio A)")
        {
            VERIFY_IS_TRUE(image == L"ep-matriarch.png");
        }
    }

    VERIFY_IS_TRUE(sawPackageKeyStep && sawPiano);
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Pictures() + L"ep-keystep (1).png") == PictureBytes(10));
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Pictures() + L"ep-keystep.png") == PictureBytes(1));
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Pictures() + L"ep-matriarch.png") == PictureBytes(11));
    VERIFY_ARE_EQUAL(LineFor(result.Lines, engine::ResultCode::DeviceNames).Count2, (uint32_t)2);

    // the temporary files are gone
    VERIFY_IS_TRUE(std::filesystem::is_empty(fixture.Folder.Temporary()));
}

void ConfigMergeTests::TestTakenLoopbackNameIsLeftOutUnlessTicked()
{
    Fixture fixture{};

    auto const id = std::wstring{ L"loopback:2:{33333333-0000-0000-0000-000000000001}" };
    auto const& item = fixture.Item(id);

    VERIFY_IS_TRUE(item.Status == engine::ItemStatus::NameTaken);
    VERIFY_IS_TRUE(item.FreeName == L"Max 2");

    std::vector<engine::ResultLine> lines{};
    auto loop = Section(fixture.Merge({}, lines), Loop);
    std::wstring key{};

    VERIFY_IS_NULL(EntryNamed(engine::jsonutil::ObjectValue(loop, L"create"), L"Max 2 (A)", key));
    VERIFY_IS_TRUE(LineFor(lines, engine::ResultCode::LoopbackLeftOut).Name == L"Max");

    engine::ImportChoices choices{};
    choices.SetAddedUnderFreeName(id, true);

    loop = Section(fixture.Merge(choices), Loop);

    auto const added = EntryNamed(engine::jsonutil::ObjectValue(loop, L"create"), L"Max 2 (A)", key);

    VERIFY_IS_NOT_NULL(added);
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(added, L"endpointB"), L"name") == L"Max 2 (B)");
}

void ConfigMergeTests::TestSameLoopbackStaysAsItIs()
{
    Fixture fixture{};

    auto const& item = fixture.Item(L"loopback:2:{DF88CA43-94B3-4795-AAAE-7CC8396B9A06}");

    VERIFY_IS_TRUE(item.Status == engine::ItemStatus::AlreadyHere);
    VERIFY_IS_TRUE(engine::OutcomeFor(item, {}) == engine::ItemOutcome::StaysAsItIs);

    auto const create = engine::jsonutil::ObjectValue(Section(fixture.Merge({}), Loop), L"create");

    // Glass to DAW and Max from here, and DAW Out from the package
    VERIFY_ARE_EQUAL(create.Size(), (uint32_t)3);
}

void ConfigMergeTests::TestTakenAssociationAndUniqueIdsGetNewOnes()
{
    Fixture fixture{};

    auto const create = engine::jsonutil::ObjectValue(Section(fixture.Merge({}), Loop), L"create");

    std::wstring key{};
    auto const dawOut = EntryNamed(create, L"DAW Out (A)", key);

    VERIFY_IS_NOT_NULL(dawOut);
    VERIFY_IS_FALSE(engine::SameText(key, L"{22222222-0000-0000-0000-000000000001}"));

    auto const uniqueA = engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(dawOut, L"endpointA"), L"uniqueIdentifier");
    auto const uniqueB = engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(dawOut, L"endpointB"), L"uniqueIdentifier");

    VERIFY_IS_FALSE(uniqueA.empty());
    VERIFY_IS_TRUE(uniqueA == uniqueB);
    VERIFY_IS_FALSE(engine::SameText(uniqueA, L"MAXHERE"));

    // and this PC's Max still has its own
    auto const max = EntryNamed(create, L"Max (A)", key);
    VERIFY_IS_TRUE(engine::SameText(key, L"{22222222-0000-0000-0000-000000000001}"));
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(engine::jsonutil::ObjectValue(max, L"endpointA"), L"uniqueIdentifier") == L"MAXHERE");
}

void ConfigMergeTests::TestHostNamedAfterOtherPcTakesThisPcsName()
{
    Fixture fixture{};

    auto const id = std::wstring{ L"host:n:{aaaaaaaa-0000-0000-0000-000000000001}" };
    auto const& item = fixture.Item(id);

    VERIFY_IS_TRUE(item.Status == engine::ItemStatus::NamedAfterOtherPc);
    VERIFY_IS_TRUE(item.FreeName == L"STAGE-LAPTOP");
    VERIFY_IS_TRUE(item.UsesOtherAdapter);
    VERIFY_IS_TRUE(engine::OutcomeFor(item, {}) == engine::ItemOutcome::TakesThisPcName);

    std::vector<engine::ResultLine> lines{};
    auto const hosts = CreateList(Section(fixture.Merge({}, lines), Network), L"hosts");

    std::wstring key{};
    auto const host = EntryNamed(hosts, L"STAGE-LAPTOP", key);

    VERIFY_IS_NOT_NULL(host);
    VERIFY_IS_FALSE(engine::SameText(key, L"{aaaaaaaa-0000-0000-0000-000000000001}"));
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(host, L"serviceInstanceName") == L"STAGE-LAPTOP_windows_midisrv");

    auto const productId = engine::jsonutil::StringValue(host, L"productInstanceId");
    VERIFY_IS_TRUE(productId.starts_with(L"winmidisrv"));
    VERIFY_IS_TRUE(productId != L"winmidisrvAAAAAAAA-0000-0000-0000-00000000");
    VERIFY_ARE_EQUAL(productId.size(), (size_t)42);

    // the adapter it was set to is on the other PC
    VERIFY_IS_TRUE(host.GetNamedBoolean(L"allowNetworkAdapterFallback", false));

    auto const& line = LineFor(lines, engine::ResultCode::HostAddedAsThisPc);
    VERIFY_IS_TRUE(line.Name == L"STUDIO-PC");
    VERIFY_IS_TRUE(line.NewName == L"STAGE-LAPTOP");
}

void ConfigMergeTests::TestHostKeepsOtherPcNameWhenUnticked()
{
    Fixture fixture{};

    auto const id = std::wstring{ L"host:n:{aaaaaaaa-0000-0000-0000-000000000001}" };

    engine::ImportChoices choices{};
    choices.SetUseThisPcName(id, false);

    VERIFY_IS_TRUE(engine::OutcomeFor(fixture.Item(id), choices) == engine::ItemOutcome::KeepsOtherPcName);

    auto const hosts = CreateList(Section(fixture.Merge(choices), Network), L"hosts");

    std::wstring key{};
    auto const host = EntryNamed(hosts, L"STUDIO-PC", key);

    VERIFY_IS_NOT_NULL(host);
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(host, L"serviceInstanceName") == L"STUDIO-PC_windows_midisrv");
}

void ConfigMergeTests::TestTakenHostNameIsLeftOutUnlessTicked()
{
    Fixture fixture{};

    auto const id = std::wstring{ L"host:n:{bbbbbbbb-0000-0000-0000-000000000002}" };
    auto const& item = fixture.Item(id);

    VERIFY_IS_TRUE(item.Status == engine::ItemStatus::NameTaken);
    VERIFY_IS_TRUE(item.FreeName == L"Practice 2");

    std::vector<engine::ResultLine> lines{};
    auto hosts = CreateList(Section(fixture.Merge({}, lines), Network), L"hosts");

    // here's Practice, and the other PC's host under this PC's name
    VERIFY_ARE_EQUAL(hosts.Size(), (uint32_t)2);
    VERIFY_IS_TRUE(LineFor(lines, engine::ResultCode::HostLeftOut).Name == L"Practice");

    engine::ImportChoices choices{};
    choices.SetAddedUnderFreeName(id, true);

    hosts = CreateList(Section(fixture.Merge(choices), Network), L"hosts");

    std::wstring key{};
    auto const host = EntryNamed(hosts, L"Practice 2", key);

    VERIFY_IS_NOT_NULL(host);
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(host, L"serviceInstanceName") == L"Practice-2");
}

void ConfigMergeTests::TestSameClientIsNotAddedTwice()
{
    Fixture fixture{};

    VERIFY_IS_TRUE(fixture.Item(L"client:n:{cccccccc-0000-0000-0000-000000000002}").Status == engine::ItemStatus::AlreadyHere);

    auto const clients = CreateList(Section(fixture.Merge({}), Network), L"clients");

    // Mac Studio once, and BomeBox
    VERIFY_ARE_EQUAL(clients.Size(), (uint32_t)2);
}

void ConfigMergeTests::TestTakenEntryIdGetsNewIdWithItsChanges()
{
    Fixture fixture{};

    auto const network = Section(fixture.Merge({}), Network);
    auto const clients = CreateList(network, L"clients");

    std::wstring key{};
    auto const bome = EntryNamed(clients, L"BomeBox", key);

    VERIFY_IS_NOT_NULL(bome);
    VERIFY_IS_FALSE(engine::SameText(key, L"{cccccccc-0000-0000-0000-000000000001}"));

    // this PC's Mac Studio keeps the ID
    std::wstring macKey{};
    VERIFY_IS_NOT_NULL(EntryNamed(clients, L"Mac Studio", macKey));
    VERIFY_IS_TRUE(engine::SameText(macKey, L"{cccccccc-0000-0000-0000-000000000001}"));

    auto const changes = engine::jsonutil::ObjectValue(
        engine::jsonutil::ObjectValue(engine::jsonutil::ObjectValue(network, L"updateEntries"), L"clients"), key);

    VERIFY_IS_NOT_NULL(changes);
    VERIFY_ARE_EQUAL(changes.GetNamedNumber(L"sendSpeedLimit", 0), 4.0);
}

void ConfigMergeTests::TestThisPcTransportSettingsAreKept()
{
    Fixture fixture{};

    auto const settings = engine::jsonutil::ObjectValue(Section(fixture.Merge({}), Network), L"transportSettings");

    VERIFY_ARE_EQUAL(settings.GetNamedNumber(L"outboundPingInterval", 0), 1000.0);
}

void ConfigMergeTests::TestRtpIsSavedWhenNotInstalled()
{
    Fixture fixture{ HereSettings, false };

    std::vector<engine::ResultLine> lines{};
    auto const rtp = Section(fixture.Merge({}, lines), Rtp);

    VERIFY_IS_NOT_NULL(rtp);
    VERIFY_ARE_EQUAL(CreateList(rtp, L"hosts").Size(), (uint32_t)1);
    VERIFY_ARE_EQUAL(LineFor(lines, engine::ResultCode::RtpSavedForLater).Count, (uint32_t)1);

    // one line for all of it, rather than one per host that isn't running yet
    for (auto const& line : lines)
    {
        if (line.Network == engine::NetworkKind::RtpMidi)
        {
            VERIFY_IS_TRUE(line.Code == engine::ResultCode::RtpSavedForLater);
        }
    }
}

void ConfigMergeTests::TestBluetoothMergesByDeviceId()
{
    Fixture fixture{};

    std::vector<engine::ResultLine> lines{};
    auto const bluetooth = Section(fixture.Merge({}, lines), Bluetooth);

    VERIFY_ARE_EQUAL(CountOf(bluetooth, L"devices"), (uint32_t)2);
    VERIFY_ARE_EQUAL(LineFor(lines, engine::ResultCode::BluetoothAdded).Count, (uint32_t)1);

    // this PC's peripheral describes this PC
    auto const peripheral = engine::jsonutil::ObjectValue(bluetooth, L"peripheral");
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(peripheral, L"advertisedName") == L"STAGE-LAPTOP");
}

void ConfigMergeTests::TestSynthModeKeepsMineByDefault()
{
    Fixture fixture{};

    auto synth = Section(fixture.Merge({}), Synth);

    VERIFY_IS_TRUE(engine::jsonutil::StringValue(synth, L"synthMode") == L"compatible");

    engine::ImportChoices choices{};
    choices.OtherSettings = engine::OtherSettingsChoice::UseThePackages;

    std::vector<engine::ResultLine> lines{};
    synth = Section(fixture.Merge(choices, lines), Synth);

    VERIFY_IS_TRUE(engine::jsonutil::StringValue(synth, L"synthMode") == L"modern");

    // the audio output depends on this PC's sound hardware
    VERIFY_IS_TRUE(engine::jsonutil::StringValue(synth, L"audioMode") == L"sharedLowLatency");
    VERIFY_IS_TRUE(LineFor(lines, engine::ResultCode::SynthModeSet).Name == L"modern");
}

void ConfigMergeTests::TestUnknownSectionCopiedOnlyWhenAbsent()
{
    {
        Fixture fixture{};

        auto const section = Section(fixture.Merge({}), Unknown);

        VERIFY_IS_NOT_NULL(section);
        VERIFY_ARE_EQUAL(section.GetNamedNumber(L"x", 0), 1.0);
    }

    {
        std::wstring here{ HereSettings };
        here.insert(here.find_last_of(L'}'), L", \"{99999999-0000-0000-0000-000000000001}\": { \"x\": 2 }");

        Fixture fixture{ here };

        VERIFY_IS_TRUE(fixture.Item(L"section:" + Unknown).Status == engine::ItemStatus::AlreadyHere);

        auto const section = Section(fixture.Merge({}), Unknown);

        VERIFY_ARE_EQUAL(section.GetNamedNumber(L"x", 0), 2.0);
    }
}

void ConfigMergeTests::TestPortNamingAutomaticNotUsableIsKept()
{
    {
        Fixture fixture{};
        fixture.System->AutomaticAvailable = false;
        fixture.Environment.AutomaticPortNamingAvailable = false;
        fixture.Contents.PortNamingValue = engine::PortNaming::Automatic;

        VERIFY_IS_TRUE(engine::MakeImportPlan(fixture.Environment, fixture.Contents, true, fixture.Plan).Succeeded());
        VERIFY_IS_FALSE(fixture.Plan.PackagePortNamingUsable);

        engine::ImportChoices choices{};
        choices.OtherSettings = engine::OtherSettingsChoice::UseThePackages;

        VERIFY_IS_FALSE(engine::PortNamingWillChange(fixture.Plan, choices));

        auto const result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, choices);

        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_IS_FALSE(result.PortNamingChanged);
        VERIFY_IS_TRUE(fixture.System->Naming == engine::PortNaming::ClassicCompatible);
        VERIFY_IS_TRUE(HasLine(result.Lines, engine::ResultCode::PortNamingNotUsable));
    }

    {
        Fixture fixture{};
        fixture.Contents.PortNamingValue = engine::PortNaming::Automatic;

        VERIFY_IS_TRUE(engine::MakeImportPlan(fixture.Environment, fixture.Contents, true, fixture.Plan).Succeeded());

        // kept by default
        auto result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, {});

        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_IS_TRUE(fixture.System->Naming == engine::PortNaming::ClassicCompatible);
        VERIFY_IS_TRUE(HasLine(result.Lines, engine::ResultCode::OtherSettingsKept));

        VERIFY_IS_TRUE(engine::MakeImportPlan(fixture.Environment, fixture.Contents, true, fixture.Plan).Succeeded());

        engine::ImportChoices choices{};
        choices.OtherSettings = engine::OtherSettingsChoice::UseThePackages;

        result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, choices);

        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_IS_TRUE(result.PortNamingChanged);
        VERIFY_IS_TRUE(fixture.System->Naming == engine::PortNaming::Automatic);
    }
}

void ConfigMergeTests::TestApplyImportBacksUpWritesAndUndoes()
{
    Fixture fixture{};

    auto const result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, {});

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.ConfigFileName == HereFileName);
    VERIFY_IS_TRUE(result.Backup.has_value());
    VERIFY_IS_TRUE(result.Backup->Reason == engine::BackupReason::BeforeImport);
    VERIFY_IS_TRUE(result.Backup->ReasonComputerName == L"STUDIO-PC");
    VERIFY_IS_TRUE(result.Backup->ReasonConfigName == L"Studio");
    VERIFY_IS_TRUE(FileExists(result.Backup->Path));

    auto const changed = ReadFileBytes(fixture.Folder.Config() + HereFileName);
    VERIFY_IS_FALSE(changed == fixture.HereBytes);
    VERIFY_IS_TRUE(engine::TakeInventory(std::string_view{ changed }).IsConfiguration);

    // the backup is listed with its reason
    auto const backups = engine::ListBackups(fixture.Environment);
    VERIFY_ARE_EQUAL(backups.size(), (size_t)1);
    VERIFY_IS_TRUE(backups.front().Reason == engine::BackupReason::BeforeImport);
    VERIFY_IS_TRUE(backups.front().ConfigFileName == HereFileName);

    // Undo this import puts back exactly what was there
    auto const undone = engine::RestoreBackup(fixture.Environment, *result.Backup, false);

    VERIFY_IS_TRUE(undone.Succeeded());
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Config() + HereFileName) == fixture.HereBytes);
    VERIFY_IS_FALSE(undone.SafetyBackup.has_value());
}

void ConfigMergeTests::TestApplyImportRefusesAChangedFile()
{
    Fixture fixture{};

    // someone else saved after the plan was made
    auto const theirs = MakeConfig(L"Live set", L"{ \"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}\": { \"update\": [] } }");
    WriteFileBytes(fixture.Folder.Config() + HereFileName, theirs);

    auto const result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, {});

    VERIFY_IS_TRUE(result.Error == engine::ConfigError::ChangedSincePlanned);
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Config() + HereFileName) == theirs);
}

void ConfigMergeTests::TestImportAsNewConfiguration()
{
    Fixture fixture{};

    VERIFY_IS_TRUE(fixture.Plan.NewConfigurationName == L"Studio (STUDIO-PC)");

    engine::ImportChoices choices{};
    choices.Target = engine::ImportTarget::NewConfiguration;

    auto const result = engine::ApplyImport(fixture.Environment, fixture.Contents, fixture.Plan, choices);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_FALSE(result.Backup.has_value());
    VERIFY_IS_TRUE(result.ConfigName == L"Studio (STUDIO-PC)");
    VERIFY_IS_TRUE(result.ConfigFileName == L"Studio (STUDIO-PC).midiconfig.json");

    // the configuration in use is untouched, and not switched away from
    VERIFY_IS_TRUE(ReadFileBytes(fixture.Folder.Config() + HereFileName) == fixture.HereBytes);
    VERIFY_IS_TRUE(fixture.System->CurrentFileName == HereFileName);

    auto const created = Parse(ReadFileBytes(fixture.Folder.Config() + result.ConfigFileName));
    auto const inventory = engine::TakeInventory(created);

    VERIFY_IS_TRUE(inventory.ConfigName == L"Studio (STUDIO-PC)");
    VERIFY_ARE_EQUAL(inventory.DeviceNames.size(), (size_t)3);
    VERIFY_IS_TRUE(CustomName(Section(created, Ks), L"dev-keystep") == L"KeyStep (Studio)");

    // still fixes a host named after the other PC
    std::wstring key{};
    VERIFY_IS_NOT_NULL(EntryNamed(CreateList(Section(created, Network), L"hosts"), L"STAGE-LAPTOP", key));
}
