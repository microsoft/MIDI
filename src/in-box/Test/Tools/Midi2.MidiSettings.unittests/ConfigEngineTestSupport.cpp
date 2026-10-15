// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>
#include <combaseapi.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include "ConfigEngineTestSupport.h"
#include "ConfigJson.h"

#include <WexTestClass.h>

namespace configtests
{
    TestFolder::TestFolder()
    {
        wchar_t temp[MAX_PATH]{};
        ::GetTempPathW(ARRAYSIZE(temp), temp);

        GUID id{};
        (void)::CoCreateGuid(&id);

        wchar_t idText[40]{};
        ::StringFromGUID2(id, idText, ARRAYSIZE(idText));

        m_root = std::wstring{ temp } + L"midisettings-tests-" + std::wstring{ idText + 1, idText + 37 } + L"\\";

        std::filesystem::create_directories(Config());
        std::filesystem::create_directories(Backups());
        std::filesystem::create_directories(Pictures());
        std::filesystem::create_directories(Temporary());
    }

    TestFolder::~TestFolder()
    {
        std::error_code ignored{};
        std::filesystem::remove_all(m_root, ignored);
    }

    _Use_decl_annotations_
    std::string ReadFileBytes(std::wstring const& path)
    {
        std::ifstream stream{ path, std::ios::binary };

        return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
    }

    _Use_decl_annotations_
    void WriteFileBytes(std::wstring const& path, std::string const& bytes)
    {
        std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    _Use_decl_annotations_
    bool FileExists(std::wstring const& path)
    {
        auto const attributes = ::GetFileAttributesW(path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    _Use_decl_annotations_
    std::string Utf8(std::wstring const& text)
    {
        return engine::jsonutil::ToUtf8(text);
    }

    _Use_decl_annotations_
    std::string MakeConfig(std::wstring const& configName, std::wstring const& settingsJson)
    {
        std::wstring text{ L"{ \"header\": { \"configName\": " };

        text += json::JsonValue::CreateStringValue(configName).Stringify();
        text += L", \"product\": \"Windows MIDI Services\", \"fileVersion\": 1 }, \"endpointTransportPluginSettings\": ";
        text += settingsJson.empty() ? std::wstring{ L"{}" } : settingsJson;
        text += L" }";

        return Utf8(text);
    }

    _Use_decl_annotations_
    json::JsonObject Parse(std::string const& bytes)
    {
        return engine::jsonutil::ParseObject(bytes);
    }

    _Use_decl_annotations_
    json::JsonObject Section(json::JsonObject const& document, std::wstring const& sectionKey)
    {
        auto const settings = engine::jsonutil::ObjectValue(document, L"endpointTransportPluginSettings");

        return engine::jsonutil::ObjectValue(settings, engine::jsonutil::FindKey(settings, sectionKey));
    }

    _Use_decl_annotations_
    std::string PictureBytes(uint8_t const seed)
    {
        // a PNG signature, then something different for each seed
        std::string bytes{ "\x89PNG\r\n\x1a\n", 8 };

        for (int i = 0; i < 64; i++)
        {
            bytes.push_back(static_cast<char>(seed + i));
        }

        return bytes;
    }

    _Use_decl_annotations_
    engine::ConfigEnvironment MakeEnvironment(
        TestFolder const& folder,
        std::shared_ptr<FakeSystem> const& system,
        std::wstring const& computerName)
    {
        engine::ConfigEnvironment environment{};

        environment.ConfigFolder = folder.Config();
        environment.BackupFolder = folder.Backups();
        environment.TemporaryFolder = folder.Temporary();
        environment.ComputerName = computerName;
        environment.AppVersion = L"test";
        environment.AutomaticPortNamingAvailable = system->AutomaticAvailable;

        environment.ReadCurrentFileName = [system]() { return system->CurrentFileName; };
        environment.WriteCurrentFileName = [system](std::wstring const& fileName) { system->CurrentFileName = fileName; return true; };
        environment.ReadPortNaming = [system]() { return system->Naming; };
        environment.WritePortNaming = [system](engine::PortNaming const naming) { system->Naming = naming; system->PortNamingWrites++; return true; };

        auto const pictures = folder.Pictures();

        environment.PicturePath = [pictures](std::wstring const& name)
            {
                auto const path = pictures + name;
                return FileExists(path) ? path : std::wstring{};
            };

        environment.StorePicture = [pictures](std::wstring const& sourcePath)
            {
                std::filesystem::path const source{ sourcePath };

                auto const leaf = source.filename().wstring();
                auto const stem = source.stem().wstring();
                auto const extension = source.extension().wstring();

                std::wstring candidate{ L"ep-" + leaf };

                for (int attempt = 1; attempt < 100; attempt++)
                {
                    auto const destination = pictures + candidate;

                    if (!FileExists(destination))
                    {
                        return ::CopyFileW(sourcePath.c_str(), destination.c_str(), TRUE) ? candidate : std::wstring{};
                    }

                    if (ReadFileBytes(destination) == ReadFileBytes(sourcePath))
                    {
                        return candidate;
                    }

                    candidate = L"ep-" + stem + L" (" + std::to_wstring(attempt) + L")" + extension;
                }

                return std::wstring{};
            };

        return environment;
    }
}
