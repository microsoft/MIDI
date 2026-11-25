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

#include <chrono>
#include <filesystem>
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include "ConfigEngineTestSupport.h"
#include "ConfigFileTests.h"

#include "ConfigBackups.h"
#include "ConfigFiles.h"
#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigPackage.h"
#include "ConfigShare.h"
#include "ConfigWrite.h"

#include <algorithm>

using namespace WEX::Logging;
using namespace configtests;

namespace
{
    std::wstring const LiveSet{ L"Live set.midiconfig.json" };

    std::wstring const PianoSettings{ LR"({
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}": { "update": [
            { "match": { "endpointDeviceId": "dev-piano" }, "customProperties": { "name": "Piano", "image": "ep-piano.png" } }
        ] }
    })" };

    FILETIME DaysAgo(FILETIME const& now, uint32_t const days)
    {
        auto const value = engine::FileTimeValue(now) - static_cast<uint64_t>(days) * 24ull * 3600ull * 10'000'000ull;

        FILETIME result{};
        result.dwLowDateTime = static_cast<DWORD>(value & 0xFFFFFFFF);
        result.dwHighDateTime = static_cast<DWORD>(value >> 32);

        return result;
    }

    FILETIME SecondsLater(FILETIME const& time, uint64_t const seconds)
    {
        auto const value = engine::FileTimeValue(time) + seconds * 10'000'000ull;

        FILETIME result{};
        result.dwLowDateTime = static_cast<DWORD>(value & 0xFFFFFFFF);
        result.dwHighDateTime = static_cast<DWORD>(value >> 32);

        return result;
    }

    void SetWriteTime(std::wstring const& path, FILETIME const& time)
    {
        auto const file = ::CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        VERIFY_IS_TRUE(file != INVALID_HANDLE_VALUE);
        VERIFY_IS_TRUE(::SetFileTime(file, &time, nullptr, &time) != FALSE);
        ::CloseHandle(file);
    }
}

void ConfigFileTests::TestWriteInPlaceRefusesAChangedFile()
{
    TestFolder folder{};

    auto const path = folder.Config() + LiveSet;
    auto const planned = MakeConfig(L"Live set", {});
    auto const theirs = MakeConfig(L"Live set", PianoSettings);

    WriteFileBytes(path, theirs);

    auto const result = engine::WriteInPlace(path, MakeConfig(L"Mine", {}), &planned);

    VERIFY_IS_TRUE(result.Error == engine::ConfigError::ChangedSincePlanned);
    VERIFY_IS_TRUE(ReadFileBytes(path) == theirs);
}

void ConfigFileTests::TestWriteInPlaceRefusesALink()
{
    TestFolder folder{};

    // a file only an administrator should be able to change, and a link to it planted where a
    // configuration would be
    auto const target = folder.Root() + L"elsewhere.txt";
    auto const link = folder.Config() + LiveSet;

    WriteFileBytes(target, "important");
    VERIFY_IS_TRUE(::CreateHardLinkW(link.c_str(), target.c_str(), nullptr) != FALSE);

    auto const result = engine::WriteInPlace(link, MakeConfig(L"Live set", {}), nullptr);

    VERIFY_IS_TRUE(result.Error == engine::ConfigError::LinkRefused);
    VERIFY_IS_TRUE(ReadFileBytes(target) == "important");
}

void ConfigFileTests::TestWriteInPlaceIdenticalBytesAreUnchanged()
{
    TestFolder folder{};

    auto const path = folder.Config() + LiveSet;
    auto const bytes = MakeConfig(L"Live set", PianoSettings);

    WriteFileBytes(path, bytes);

    auto const old = DaysAgo(engine::CurrentFileTime(), 3);
    SetWriteTime(path, old);

    auto const result = engine::WriteInPlace(path, bytes, &bytes);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(result.Unchanged);

    WIN32_FILE_ATTRIBUTE_DATA data{};
    VERIFY_IS_TRUE(::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) != FALSE);
    VERIFY_ARE_EQUAL(engine::FileTimeValue(data.ftLastWriteTime), engine::FileTimeValue(old));
}

void ConfigFileTests::TestWriteInPlaceWaitsOutASharingViolation()
{
    TestFolder folder{};

    auto const path = folder.Config() + LiveSet;
    WriteFileBytes(path, MakeConfig(L"Live set", {}));

    // another program has the file open, as the API does while it saves, and lets go shortly
    auto const busy = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    VERIFY_IS_TRUE(busy != INVALID_HANDLE_VALUE);

    std::thread release{ [busy]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            ::CloseHandle(busy);
        } };

    auto const wanted = MakeConfig(L"Live set", PianoSettings);
    auto const result = engine::WriteInPlace(path, wanted, nullptr);

    release.join();

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(ReadFileBytes(path) == wanted);
}

void ConfigFileTests::TestBackupsListFullAndConfigurationOnly()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    auto const now = engine::CurrentFileTime();

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", PianoSettings));

    // made by Windows MIDI Services before a day's first change, and by an earlier MIDI Settings
    auto const daily = folder.Config() + LiveSet + L".2026-10-13.bak";
    auto const earlier = folder.Config() + LiveSet + L".2026-10-12-101500.bak";

    WriteFileBytes(daily, MakeConfig(L"Live set", {}));
    WriteFileBytes(earlier, MakeConfig(L"Live set", {}));
    SetWriteTime(daily, DaysAgo(now, 1));
    SetWriteTime(earlier, DaysAgo(now, 2));

    // and things that only look like backups
    WriteFileBytes(folder.Config() + LiveSet + L".old.bak", "{}");
    WriteFileBytes(folder.Config() + L"notes.txt.2026-10-13.bak", "{}");

    auto const made = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});

    VERIFY_IS_TRUE(made.Succeeded());
    VERIFY_IS_TRUE(made.Backup.FileName.starts_with(L"Live set "));
    VERIFY_IS_TRUE(made.Backup.FileName.ends_with(L".midiconfigpack"));

    auto const backups = engine::ListBackups(environment);

    VERIFY_ARE_EQUAL(backups.size(), (size_t)3);

    VERIFY_IS_TRUE(backups[0].Kind == engine::BackupKind::Full);
    VERIFY_IS_TRUE(backups[0].Reason == engine::BackupReason::YouBackedItUp);
    VERIFY_IS_TRUE(backups[0].ConfigName == L"Live set");
    VERIFY_IS_TRUE(backups[0].ConfigFileName == LiveSet);

    VERIFY_IS_TRUE(backups[1].Kind == engine::BackupKind::Daily);
    VERIFY_IS_TRUE(backups[1].IsConfigurationOnly());
    VERIFY_IS_TRUE(backups[1].ConfigFileName == LiveSet);

    VERIFY_IS_TRUE(backups[2].Kind == engine::BackupKind::Earlier);
    VERIFY_IS_TRUE(backups[2].IsConfigurationOnly());
}

void ConfigFileTests::TestBackupNamesNeverOverwrite()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", {}));

    auto const first = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});
    auto const second = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});
    auto const third = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});

    VERIFY_IS_TRUE(first.Succeeded() && second.Succeeded() && third.Succeeded());
    VERIFY_IS_TRUE(first.Backup.Path != second.Backup.Path);
    VERIFY_IS_TRUE(second.Backup.Path != third.Backup.Path);
    VERIFY_IS_TRUE(first.Backup.Path != third.Backup.Path);

    VERIFY_ARE_EQUAL(engine::ListBackups(environment).size(), (size_t)3);

    // nothing is left half written
    for (auto const& entry : std::filesystem::directory_iterator{ folder.Backups() })
    {
        VERIFY_IS_FALSE(entry.path().wstring().ends_with(L".partial"));
    }
}

void ConfigFileTests::TestRestoreFullBackupBringsPicturesAndPortNames()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    auto const original = MakeConfig(L"Live set", PianoSettings);

    WriteFileBytes(folder.Config() + LiveSet, original);
    WriteFileBytes(folder.Pictures() + L"ep-piano.png", PictureBytes(2));
    system->Naming = engine::PortNaming::NewStyle;
    system->CurrentFileName = LiveSet;

    auto const backup = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});
    VERIFY_IS_TRUE(backup.Succeeded());

    // things go wrong afterwards
    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", {}));
    VERIFY_IS_TRUE(::DeleteFileW((folder.Pictures() + L"ep-piano.png").c_str()) != FALSE);
    system->Naming = engine::PortNaming::ClassicCompatible;

    auto const restored = engine::RestoreBackup(environment, backup.Backup, true);

    VERIFY_IS_TRUE(restored.Succeeded());
    VERIFY_IS_TRUE(ReadFileBytes(folder.Config() + LiveSet) == original);
    VERIFY_IS_TRUE(ReadFileBytes(folder.Pictures() + L"ep-piano.png") == PictureBytes(2));
    VERIFY_IS_TRUE(system->Naming == engine::PortNaming::NewStyle);
    VERIFY_IS_TRUE(restored.PortNamingChanged);

    // what was there is backed up first, so the restore can be undone too
    VERIFY_IS_TRUE(restored.SafetyBackup.has_value());
    VERIFY_IS_TRUE(restored.SafetyBackup->Reason == engine::BackupReason::BeforeRestore);
    VERIFY_IS_TRUE(FileExists(restored.SafetyBackup->Path));
}

void ConfigFileTests::TestRestoreConfigurationOnlyBackup()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    auto const older = MakeConfig(L"Live set", PianoSettings);

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", {}));
    WriteFileBytes(folder.Config() + LiveSet + L".2026-10-13.bak", older);
    system->Naming = engine::PortNaming::NewStyle;

    auto const backups = engine::ListBackups(environment);
    VERIFY_ARE_EQUAL(backups.size(), (size_t)1);

    auto const restored = engine::RestoreBackup(environment, backups.front(), true);

    VERIFY_IS_TRUE(restored.Succeeded());
    VERIFY_IS_TRUE(ReadFileBytes(folder.Config() + LiveSet) == older);

    // only the configuration: settings that live outside it are left alone
    VERIFY_IS_TRUE(system->Naming == engine::PortNaming::NewStyle);
    VERIFY_IS_FALSE(restored.PortNamingChanged);
    VERIFY_ARE_EQUAL(system->PortNamingWrites, (uint32_t)0);
}

void ConfigFileTests::TestDeleteOldBackupsKeepsNewestOfEach()
{
    auto const now = engine::CurrentFileTime();

    std::vector<engine::BackupEntry> backups{};

    auto const add = [&backups, &now](std::wstring const& configFileName, uint32_t const daysAgo)
        {
            engine::BackupEntry entry{};
            entry.ConfigFileName = configFileName;
            entry.MadeAt = DaysAgo(now, daysAgo);
            entry.Path = configFileName + L"-" + std::to_wstring(daysAgo);
            backups.push_back(entry);
        };

    // Live set: 8 backups, 3 of them older than 30 days
    for (uint32_t days : { 1u, 2u, 5u, 10u, 20u, 40u, 50u, 60u })
    {
        add(L"Live set.midiconfig.json", days);
    }

    // Rehearsal: only 3, all old, so all of them are among its newest 5
    for (uint32_t days : { 100u, 200u, 300u })
    {
        add(L"Rehearsal.midiconfig.json", days);
    }

    auto const old = engine::OldBackups(backups, now, 30, 5);

    VERIFY_ARE_EQUAL(old.size(), (size_t)3);

    for (auto const& entry : old)
    {
        VERIFY_IS_TRUE(entry.ConfigFileName == L"Live set.midiconfig.json");
        VERIFY_IS_TRUE(engine::FileTimeValue(entry.MadeAt) < engine::FileTimeValue(DaysAgo(now, 30)));
    }

    // keeping none of each by count leaves only the time window
    VERIFY_ARE_EQUAL(engine::OldBackups(backups, now, 30, 0).size(), (size_t)6);
}

void ConfigFileTests::TestBackupBeforeLastChange()
{
    auto const now = engine::CurrentFileTime();

    engine::BackupEntry imported{};
    imported.Kind = engine::BackupKind::Full;
    imported.ConfigFileName = LiveSet;
    imported.MadeAt = now;
    imported.Reason = engine::BackupReason::BeforeImport;
    imported.ReasonComputerName = L"STUDIO-PC";

    std::vector<engine::BackupEntry> backups{ imported };

    auto const found = engine::BackupBeforeLastChange(LiveSet, SecondsLater(now, 5), backups);

    VERIFY_IS_TRUE(found.has_value());
    VERIFY_IS_TRUE(found->ReasonComputerName == L"STUDIO-PC");

    // changed again much later, by something else
    VERIFY_IS_FALSE(engine::BackupBeforeLastChange(LiveSet, SecondsLater(now, 3600), backups).has_value());

    // a backup the customer made isn't a change
    backups.front().Reason = engine::BackupReason::YouBackedItUp;
    VERIFY_IS_FALSE(engine::BackupBeforeLastChange(LiveSet, SecondsLater(now, 5), backups).has_value());
}

void ConfigFileTests::TestCreateRenameDuplicateSwitchDelete()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", PianoSettings));
    system->CurrentFileName = LiveSet;

    auto const rehearsal = engine::CreateConfiguration(environment, L"  Rehearsal  ", {});
    VERIFY_IS_TRUE(rehearsal.Succeeded());
    VERIFY_IS_TRUE(rehearsal.FileName == L"Rehearsal.midiconfig.json");

    // creating isn't switching
    VERIFY_IS_TRUE(system->CurrentFileName == LiveSet);

    auto const second = engine::CreateConfiguration(environment, L"Rehearsal", {});
    VERIFY_IS_TRUE(second.Succeeded());
    VERIFY_IS_TRUE(second.FileName == L"Rehearsal 2.midiconfig.json");

    VERIFY_IS_TRUE(engine::CreateConfiguration(environment, L"   ", {}).Error == engine::ConfigError::NameNotUsable);

    VERIFY_IS_TRUE(engine::RenameConfiguration(environment, second.FileName, L"Encore").Succeeded());

    auto const copy = engine::CreateConfiguration(environment, L"Copy of Live set", LiveSet);
    VERIFY_IS_TRUE(copy.Succeeded());

    auto const files = engine::ListConfigurations(environment);
    VERIFY_ARE_EQUAL(files.size(), (size_t)4);

    // by name: Copy of Live set, Encore, Live set, Rehearsal
    VERIFY_IS_TRUE(files[0].ConfigName == L"Copy of Live set");
    VERIFY_ARE_EQUAL(files[0].Inventory.DeviceNames.size(), (size_t)1);
    VERIFY_IS_TRUE(files[1].ConfigName == L"Encore");
    VERIFY_IS_TRUE(files[1].FileName == L"Rehearsal 2.midiconfig.json");
    VERIFY_IS_TRUE(files[2].IsCurrent);
    VERIFY_IS_TRUE(files[3].Inventory.IsEmpty());

    VERIFY_IS_TRUE(engine::SwitchConfiguration(environment, rehearsal.FileName).Succeeded());
    VERIFY_IS_TRUE(system->CurrentFileName == rehearsal.FileName);
    VERIFY_IS_TRUE(engine::SwitchConfiguration(environment, L"Missing.midiconfig.json").Error == engine::ConfigError::NotFound);

    // the one in use can't be deleted
    VERIFY_IS_TRUE(engine::DeleteConfiguration(environment, rehearsal.FileName).Error == engine::ConfigError::InUse);

    VERIFY_IS_TRUE(engine::DeleteConfiguration(environment, copy.FileName).Succeeded());
    VERIFY_IS_FALSE(FileExists(folder.Config() + copy.FileName));

    // and it was backed up first
    auto const backups = engine::ListBackups(environment);
    VERIFY_ARE_EQUAL(backups.size(), (size_t)1);
    VERIFY_IS_TRUE(backups.front().Reason == engine::BackupReason::BeforeDelete);
    VERIFY_IS_TRUE(backups.front().ConfigFileName == copy.FileName);
}

void ConfigFileTests::TestPortNamingNames()
{
    for (auto const naming : { engine::PortNaming::ClassicCompatible, engine::PortNaming::NewStyle, engine::PortNaming::Automatic })
    {
        auto const name = engine::PortNamingName(naming);

        VERIFY_IS_FALSE(name.empty());
        VERIFY_IS_TRUE(engine::PortNamingFromName(name) == naming);
    }

    VERIFY_IS_TRUE(engine::PortNamingName(engine::PortNaming::ClassicCompatible) == L"classicCompatible");
    VERIFY_IS_TRUE(engine::PortNamingName(engine::PortNaming::NewStyle) == L"newStyle");
    VERIFY_IS_TRUE(engine::PortNamingName(engine::PortNaming::Automatic) == L"automatic");
    VERIFY_IS_TRUE(engine::PortNamingFromName(L"NEWSTYLE") == engine::PortNaming::NewStyle);
    VERIFY_IS_TRUE(engine::PortNamingFromName(L"2") == engine::PortNaming::None);
    VERIFY_IS_TRUE(engine::PortNamingFromName(L"") == engine::PortNaming::None);
}

void ConfigFileTests::TestShareTakesOnlyTheChosenParts()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    std::wstring const settings{ LR"json({
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}": { "update": [
            { "match": { "endpointDeviceId": "dev-piano" }, "customProperties": { "name": "Piano", "image": "ep-piano.png" } }
        ] },
        "{942BF02D-93C0-4EA8-B03E-D51156CA75E1}": { "create": {
            "{11111111-2222-3333-4444-555555555555}": {
                "endpointA": { "name": "Max (A)", "uniqueIdentifier": "max" },
                "endpointB": { "name": "Max (B)", "uniqueIdentifier": "max" } } } }
    })json" };

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", settings));
    WriteFileBytes(folder.Pictures() + L"ep-piano.png", PictureBytes(3));
    system->Naming = engine::PortNaming::NewStyle;

    // the port names setting lives outside the file, so Other settings can always go
    auto const inventory = engine::TakeInventory(std::string_view{ ReadFileBytes(folder.Config() + LiveSet) });
    auto const shareable = engine::ShareableParts(environment, inventory);

    VERIFY_IS_TRUE((shareable & engine::PartBit(engine::ConfigPart::OtherSettings)) != 0);
    VERIFY_IS_TRUE((shareable & engine::PartBit(engine::ConfigPart::Bluetooth)) == 0);

    // loopbacks only: no device names, no pictures, no port names
    auto const loopbacksOnly = folder.Root() + L"loopbacks.midiconfigpack";
    auto const first = engine::ShareConfiguration(environment, LiveSet, engine::PartBit(engine::ConfigPart::Loopbacks), L"Loops", L"for the band", loopbacksOnly);

    VERIFY_IS_TRUE(first.Succeeded());
    VERIFY_ARE_EQUAL(first.Pictures, (uint32_t)0);

    engine::PackageContents contents{};
    VERIFY_IS_TRUE(engine::ReadPackage(loopbacksOnly, contents).Succeeded());
    VERIFY_IS_TRUE(contents.Info.ConfigName == L"Loops");
    VERIFY_IS_TRUE(contents.Info.Note == L"for the band");
    VERIFY_IS_TRUE(contents.Info.ComputerName == L"STAGE-LAPTOP");
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::None);
    VERIFY_IS_TRUE(contents.Pictures.empty());

    auto const shared = engine::TakeInventory(std::string_view{ contents.ConfigurationBytes });
    VERIFY_IS_TRUE(shared.DeviceNames.empty());
    VERIFY_ARE_EQUAL(shared.Loopbacks.size(), (size_t)1);

    // device names and other settings: the picture and the port names go along
    auto const namesOnly = folder.Root() + L"names.midiconfigpack";
    auto const second = engine::ShareConfiguration(
        environment,
        LiveSet,
        engine::PartBit(engine::ConfigPart::DeviceNames) | engine::PartBit(engine::ConfigPart::OtherSettings),
        L"",
        L"",
        namesOnly);

    VERIFY_IS_TRUE(second.Succeeded());
    VERIFY_ARE_EQUAL(second.Pictures, (uint32_t)1);

    VERIFY_IS_TRUE(engine::ReadPackage(namesOnly, contents).Succeeded());
    VERIFY_IS_TRUE(contents.Info.ConfigName == L"Live set");
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::NewStyle);
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)1);

    auto const named = engine::TakeInventory(std::string_view{ contents.ConfigurationBytes });
    VERIFY_ARE_EQUAL(named.DeviceNames.size(), (size_t)1);
    VERIFY_IS_TRUE(named.Loopbacks.empty());

    // nothing chosen is refused rather than saved as an empty package
    auto const nothing = engine::ShareConfiguration(environment, LiveSet, 0, L"", L"", folder.Root() + L"nothing.midiconfigpack");
    VERIFY_IS_TRUE(nothing.Error == engine::ConfigError::Empty);
    VERIFY_IS_FALSE(FileExists(folder.Root() + L"nothing.midiconfigpack"));
}

void ConfigFileTests::TestShareBackup()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    WriteFileBytes(folder.Config() + LiveSet, MakeConfig(L"Live set", PianoSettings));
    WriteFileBytes(folder.Pictures() + L"ep-piano.png", PictureBytes(4));

    // a full backup is already a package, so it goes as it is
    auto const backup = engine::MakeBackup(environment, LiveSet, engine::BackupReason::YouBackedItUp, {}, {});
    VERIFY_IS_TRUE(backup.Succeeded());

    auto const fullPath = folder.Root() + L"full.midiconfigpack";
    auto const full = engine::ShareBackup(environment, backup.Backup, fullPath);

    VERIFY_IS_TRUE(full.Succeeded());
    VERIFY_IS_TRUE(ReadFileBytes(fullPath) == ReadFileBytes(backup.Backup.Path));

    // a configuration-only one gets the pictures still on this PC
    WriteFileBytes(folder.Config() + LiveSet + L".2026-10-13.bak", MakeConfig(L"Live set", PianoSettings));

    auto const backups = engine::ListBackups(environment);
    auto const daily = std::find_if(backups.begin(), backups.end(), [](auto const& entry) { return entry.Kind == engine::BackupKind::Daily; });
    VERIFY_IS_TRUE(daily != backups.end());

    auto const dailyPath = folder.Root() + L"daily.midiconfigpack";
    auto const shared = engine::ShareBackup(environment, *daily, dailyPath);

    VERIFY_IS_TRUE(shared.Succeeded());
    VERIFY_ARE_EQUAL(shared.Pictures, (uint32_t)1);

    engine::PackageContents contents{};
    VERIFY_IS_TRUE(engine::ReadPackage(dailyPath, contents).Succeeded());
    VERIFY_IS_TRUE(contents.Info.Kind == engine::PackageKind::Share);
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::None);
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)1);
}
