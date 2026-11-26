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

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include "ConfigEngineTestSupport.h"
#include "ConfigPackageTests.h"

#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigPackage.h"
#include "ConfigPictures.h"
#include "ZipArchive.h"

using namespace WEX::Logging;
using namespace configtests;

namespace
{
    std::wstring const SampleSettings{ LR"json({
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}": {
            "update": [
                { "match": { "endpointDeviceId": "\\\\?\\swd#midisrv#midiu_ks_1", "transportSuppliedEndpointName": "Arturia KeyStep 37" },
                  "customProperties": { "name": "KeyStep", "image": "ep-keystep.png" } }
            ]
        },
        "{942BF02D-93C0-4EA8-B03E-D51156CA75E1}": {
            "create": {
                "{11111111-1111-1111-1111-111111111111}": {
                    "endpointA": { "name": "Max (A)", "uniqueIdentifier": "MAX", "image": "ep-max.png" },
                    "endpointB": { "name": "Max (B)", "uniqueIdentifier": "MAX" }
                }
            }
        },
        "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": {
            "create": { "hosts": { "{aaaaaaaa-0000-0000-0000-000000000001}": { "name": "STUDIO-PC", "serviceInstanceName": "STUDIO-PC_windows_midisrv" } } },
            "transportSettings": { "outboundPingInterval": 2000 }
        },
        "{5DC87270-F318-4838-A4F9-6AADC63E925F}": { "devices": [ { "deviceId": "112233445566", "name": "LUMI" } ] },
        "{7605713E-FEA9-409D-A90F-A81233200D0A}": { "synthMode": "modern", "audioMode": "shared" },
        "{12345678-1234-1234-1234-123456789ABC}": { "something": true }
    })json" };

    std::vector<uint8_t> Bytes(std::string const& text)
    {
        return std::vector<uint8_t>{ text.begin(), text.end() };
    }

    std::vector<uint8_t> PackageJson(int const version)
    {
        return Bytes("{ \"formatVersion\": " + std::to_string(version) + ", \"kind\": \"share\", \"configName\": \"x\", \"computerName\": \"STUDIO-PC\" }");
    }

    engine::ConfigResult ReadZip(std::vector<midiapp::ZipEntry> const& entries, engine::PackageContents& contents)
    {
        auto const zip = midiapp::BuildZip(entries, midiapp::ZipCompression::Store);

        VERIFY_IS_FALSE(zip.empty());

        return engine::ReadPackageFromBytes(std::span<uint8_t const>{ zip.data(), zip.size() }, contents);
    }
}

void ConfigPackageTests::TestRoundTripKeepsEverything()
{
    TestFolder folder{};
    auto const system = std::make_shared<FakeSystem>();
    auto const environment = MakeEnvironment(folder, system);

    WriteFileBytes(folder.Pictures() + L"ep-keystep.png", PictureBytes(1));
    WriteFileBytes(folder.Pictures() + L"ep-max.png", PictureBytes(2));

    engine::PackageSource source{};
    source.Info.Kind = engine::PackageKind::Share;
    source.Info.ConfigName = L"Live set";
    source.Info.Note = L"Everything from the stage laptop";
    source.Info.ComputerName = L"STAGE-LAPTOP";
    source.Info.AppVersion = L"test";
    source.Info.Parts = engine::PartBit(engine::ConfigPart::All);
    source.ConfigurationBytes = MakeConfig(L"Live set", SampleSettings);
    source.PortNamingValue = engine::PortNaming::NewStyle;
    source.Pictures = engine::PictureFiles(environment, { L"ep-keystep.png", L"ep-max.png" });

    VERIFY_ARE_EQUAL(source.Pictures.size(), (size_t)2);

    auto const path = folder.Root() + L"Live set.midiconfigpack";

    VERIFY_IS_TRUE(engine::WritePackage(path, source).Succeeded());
    VERIFY_IS_TRUE(FileExists(path));
    VERIFY_IS_FALSE(FileExists(path + L".partial"));

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Succeeded());
    VERIFY_IS_FALSE(contents.IsPlainConfiguration);
    VERIFY_IS_TRUE(contents.Info.ConfigName == L"Live set");
    VERIFY_IS_TRUE(contents.Info.Note == L"Everything from the stage laptop");
    VERIFY_IS_TRUE(contents.Info.ComputerName == L"STAGE-LAPTOP");
    VERIFY_IS_TRUE(contents.Info.HasCreated);
    VERIFY_ARE_EQUAL(contents.Info.Parts, engine::PartBit(engine::ConfigPart::All));
    VERIFY_ARE_EQUAL(contents.Info.PictureCount, (uint32_t)2);
    VERIFY_IS_TRUE(contents.ConfigurationBytes == source.ConfigurationBytes);
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::NewStyle);
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)2);

    auto const keystep = contents.FindPicture(L"ep-keystep.png");
    VERIFY_IS_NOT_NULL(keystep);
    VERIFY_IS_TRUE(std::string(keystep->begin(), keystep->end()) == PictureBytes(1));
}

void ConfigPackageTests::TestConfigurationBytesAreExact()
{
    TestFolder folder{};

    // a byte order mark, tabs and odd spacing, all of which have to survive untouched
    std::string const original = "\xEF\xBB\xBF{\t\"header\" : {\"configName\":\"Odd\"},\r\n\"endpointTransportPluginSettings\":{   }}";

    engine::PackageSource source{};
    source.Info.ConfigName = L"Odd";
    source.Info.Parts = engine::PartBit(engine::ConfigPart::All);
    source.ConfigurationBytes = original;

    auto const path = folder.Root() + L"odd.midiconfigpack";

    VERIFY_IS_TRUE(engine::WritePackage(path, source).Succeeded());

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Succeeded());
    VERIFY_IS_TRUE(contents.ConfigurationBytes == original);
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::None);
}

void ConfigPackageTests::TestMissingPictureIsLeftOut()
{
    TestFolder folder{};

    engine::PackageSource source{};
    source.Info.ConfigName = L"Live set";
    source.Info.Parts = engine::PartBit(engine::ConfigPart::All);
    source.ConfigurationBytes = MakeConfig(L"Live set", SampleSettings);
    source.Pictures = { { L"ep-keystep.png", folder.Pictures() + L"not-there.png" } };

    auto const path = folder.Root() + L"p.midiconfigpack";

    VERIFY_IS_TRUE(engine::WritePackage(path, source).Succeeded());

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Succeeded());
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)0);
    VERIFY_ARE_EQUAL(contents.Info.PictureCount, (uint32_t)0);
}

void ConfigPackageTests::TestPlainConfigurationIsAccepted()
{
    TestFolder folder{};

    auto const bytes = MakeConfig(L"Old one", SampleSettings);
    auto const path = folder.Root() + L"Old one.midiconfig.json";

    WriteFileBytes(path, bytes);

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Succeeded());
    VERIFY_IS_TRUE(contents.IsPlainConfiguration);
    VERIFY_IS_TRUE(contents.ConfigurationBytes == bytes);
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)0);
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::None);
    VERIFY_IS_TRUE(contents.Info.ConfigName == L"Old one");
}

void ConfigPackageTests::TestOtherJsonIsRefused()
{
    TestFolder folder{};

    auto const path = folder.Root() + L"other.json";
    WriteFileBytes(path, "{ \"settings\": { \"a\": 1 } }");

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Error == engine::ConfigError::NotAPackage);

    // and a zip that isn't a package
    VERIFY_IS_TRUE(ReadZip({ { L"readme.txt", Bytes("hello") } }, contents).Error == engine::ConfigError::NotAPackage);
}

void ConfigPackageTests::TestNewerFormatIsRefused()
{
    engine::PackageContents contents{};

    auto const result = ReadZip({
        { L"package.json", PackageJson(2) },
        { L"configuration.json", Bytes(MakeConfig(L"x", {})) } }, contents);

    VERIFY_IS_TRUE(result.Error == engine::ConfigError::NewerPackage);
    VERIFY_IS_TRUE(contents.ConfigurationBytes.empty());
}

void ConfigPackageTests::TestTooManyEntriesIsRefused()
{
    std::vector<midiapp::ZipEntry> entries{
        { L"package.json", PackageJson(1) },
        { L"configuration.json", Bytes(MakeConfig(L"x", {})) } };

    for (int i = 0; i < 1100; i++)
    {
        entries.push_back({ L"pictures/p" + std::to_wstring(i) + L".png", Bytes("x") });
    }

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(ReadZip(entries, contents).Error == engine::ConfigError::TooLarge);
}

void ConfigPackageTests::TestOversizedConfigurationIsRefused()
{
    // valid JSON, one byte past the limit
    std::string big = MakeConfig(L"x", {});
    big.insert(1, std::string(static_cast<size_t>(engine::MaximumConfigBytes) + 1 - big.size(), ' '));

    VERIFY_ARE_EQUAL(big.size(), static_cast<size_t>(engine::MaximumConfigBytes) + 1);

    engine::PackageContents contents{};

    auto const result = ReadZip({
        { L"package.json", PackageJson(1) },
        { L"configuration.json", Bytes(big) } }, contents);

    VERIFY_IS_TRUE(result.Error == engine::ConfigError::TooLarge);
}

void ConfigPackageTests::TestDamagedChecksumIsRefused()
{
    auto const zip = midiapp::BuildZip({
        { L"package.json", PackageJson(1) },
        { L"configuration.json", Bytes(MakeConfig(L"MARKERNAME", {})) } }, midiapp::ZipCompression::Store);

    std::string text{ zip.begin(), zip.end() };
    auto const at = text.find("MARKERNAME");

    VERIFY_IS_TRUE(at != std::string::npos);

    auto damaged = zip;
    damaged[at] = static_cast<uint8_t>('N');

    engine::PackageContents contents{};

    VERIFY_IS_TRUE(engine::ReadPackageFromBytes(std::span<uint8_t const>{ damaged.data(), damaged.size() }, contents).Error == engine::ConfigError::Damaged);
}

void ConfigPackageTests::TestPathNamesAreNeverUsed()
{
    engine::PackageContents contents{};

    auto const result = ReadZip({
        { L"package.json", PackageJson(1) },
        { L"configuration.json", Bytes(MakeConfig(L"x", {})) },
        { L"pictures/../evil.png", Bytes("e1") },
        { L"pictures/C:evil.png", Bytes("e2") },
        { L"pictures/sub/evil.png", Bytes("e3") },
        { L"pictures/evil.exe", Bytes("e4") },
        { L"pictures/..png", Bytes("e5") },
        { L"../pictures/evil.png", Bytes("e6") },
        { L"/pictures/evil.png", Bytes("e7") },
        { L"pictures/good.png", Bytes("ok") } }, contents);

    // a reader may refuse such a zip outright; when it doesn't, only the good picture is read
    if (result.Succeeded())
    {
        VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)1);
        VERIFY_IS_TRUE(contents.Pictures.front().first == L"good.png");
    }
    else
    {
        Log::Comment(L"The zip reader refused the package outright.");
        VERIFY_IS_TRUE(contents.Pictures.empty());
    }

    VERIFY_IS_FALSE(engine::IsUsablePictureName(L"..\\x.png"));
    VERIFY_IS_FALSE(engine::IsUsablePictureName(L"C:x.png"));
    VERIFY_IS_FALSE(engine::IsUsablePictureName(L"x.png "));
    VERIFY_IS_FALSE(engine::IsUsablePictureName(L"x.gif"));
    VERIFY_IS_TRUE(engine::IsUsablePictureName(L"ep-KeyStep (1).PNG"));
    VERIFY_IS_TRUE(engine::IsUsablePictureName(L"logo.svg"));
}

void ConfigPackageTests::TestUnknownEntriesAreIgnored()
{
    engine::PackageContents contents{};

    auto const result = ReadZip({
        { L"package.json", PackageJson(1) },
        { L"configuration.json", Bytes(MakeConfig(L"x", {})) },
        { L"settings.reg", Bytes("Windows Registry Editor Version 5.00") },
        { L"readme.txt", Bytes("hello") },
        { L"configuration.json.bak", Bytes("{}") } }, contents);

    VERIFY_IS_TRUE(result.Succeeded());
    VERIFY_IS_TRUE(contents.PortNamingValue == engine::PortNaming::None);
    VERIFY_ARE_EQUAL(contents.Pictures.size(), (size_t)0);
}

void ConfigPackageTests::TestPortNamingIsStoredByName()
{
    TestFolder folder{};

    struct Case { engine::PortNaming Naming; char const* Name; };

    for (auto const& test : { Case{ engine::PortNaming::ClassicCompatible, "classicCompatible" }, Case{ engine::PortNaming::NewStyle, "newStyle" }, Case{ engine::PortNaming::Automatic, "automatic" } })
    {
        engine::PackageSource source{};
        source.Info.ConfigName = L"x";
        source.ConfigurationBytes = MakeConfig(L"x", {});
        source.PortNamingValue = test.Naming;

        auto const path = folder.Root() + L"naming.midiconfigpack";

        VERIFY_IS_TRUE(engine::WritePackage(path, source).Succeeded());

        midiapp::ZipReader reader{};
        VERIFY_IS_TRUE(reader.OpenFile(std::filesystem::path{ path }, 16) == midiapp::ZipStatus::Read);

        bool found{ false };

        for (auto const& item : reader.Items())
        {
            if (item.Name != L"settings.json")
            {
                continue;
            }

            std::vector<uint8_t> bytes{};
            VERIFY_IS_TRUE(reader.Extract(item, 4096, bytes) == midiapp::ZipStatus::Read);

            std::string const text{ bytes.begin(), bytes.end() };

            // by name, never as a number or as registry data
            VERIFY_IS_TRUE(text.find(test.Name) != std::string::npos);
            VERIFY_IS_TRUE(text.find_first_of("0123456789") == std::string::npos);
            found = true;
        }

        VERIFY_IS_TRUE(found);

        engine::PackageContents contents{};
        VERIFY_IS_TRUE(engine::ReadPackage(path, contents).Succeeded());
        VERIFY_IS_TRUE(contents.PortNamingValue == test.Naming);
    }
}

void ConfigPackageTests::TestPartsCanBeLeftOut()
{
    auto const document = Parse(MakeConfig(L"Live set", SampleSettings));

    auto const parts = engine::PartBit(engine::ConfigPart::DeviceNames) | engine::PartBit(engine::ConfigPart::Loopbacks);
    auto const trimmed = engine::WithOnlyParts(document, parts);

    VERIFY_IS_NOT_NULL(trimmed);

    auto const inventory = engine::TakeInventory(trimmed);

    VERIFY_ARE_EQUAL(inventory.DeviceNames.size(), (size_t)1);
    VERIFY_ARE_EQUAL(inventory.Loopbacks.size(), (size_t)1);
    VERIFY_ARE_EQUAL(inventory.Hosts.size(), (size_t)0);
    VERIFY_ARE_EQUAL(inventory.BluetoothDevices.size(), (size_t)0);
    VERIFY_IS_TRUE(inventory.SynthMode.empty());
    VERIFY_ARE_EQUAL(inventory.OtherSections.size(), (size_t)0);

    // the original is untouched
    VERIFY_ARE_EQUAL(engine::TakeInventory(document).Hosts.size(), (size_t)1);

    auto const pictures = engine::PicturesUsedBy(engine::TakeInventory(document), engine::PartBit(engine::ConfigPart::DeviceNames));
    VERIFY_ARE_EQUAL(pictures.size(), (size_t)1);
    VERIFY_IS_TRUE(pictures.front() == L"ep-keystep.png");
}
