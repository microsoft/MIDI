// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "ConfigFiles.h"
#include "ConfigBackups.h"
#include "ConfigJson.h"
#include "ConfigWrite.h"

#include <algorithm>
#include <cwctype>

namespace midisettings::configengine
{
    namespace
    {
        constexpr size_t MaximumConfigNameLength{ 100 };
        constexpr uint32_t MaximumNameAttempts{ 999 };

        std::wstring StemOf(_In_ std::wstring const& fileName)
        {
            if (EndsWithText(fileName, ConfigFileSuffix))
            {
                return fileName.substr(0, fileName.size() - ConfigFileSuffix.size());
            }

            return fileName;
        }

        bool FileExists(_In_ std::wstring const& path) noexcept
        {
            auto const attributes = ::GetFileAttributesW(path.c_str());

            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        bool IsConfigFileName(_In_ std::wstring const& fileName) noexcept
        {
            return IsUsableFileName(fileName) &&
                fileName.size() > ConfigFileSuffix.size() &&
                EndsWithText(fileName, ConfigFileSuffix);
        }

        CreateConfigResult WriteNewConfiguration(
            _In_ ConfigEnvironment const& environment,
            _In_ std::wstring const& configName,
            _In_ std::string const& bytes)
        {
            CreateConfigResult result{};

            if (bytes.empty())
            {
                result.Error = ConfigError::Unexpected;
                return result;
            }

            // CREATE_NEW, and a new name when another file appears under the chosen one first
            for (uint32_t attempt = 0; attempt < 3; attempt++)
            {
                result.FileName = FileNameForNewConfiguration(environment, configName);

                if (result.FileName.empty())
                {
                    result.Error = ConfigError::NameNotUsable;
                    return result;
                }

                auto const written = WriteNewFile(environment.ConfigFolder + result.FileName, bytes);

                if (written.Error == ConfigError::NameTaken)
                {
                    continue;
                }

                result.Error = written.Error;
                result.SystemError = written.SystemError;

                return result;
            }

            result.Error = ConfigError::NameTaken;
            return result;
        }
    }

    _Use_decl_annotations_
    std::wstring CleanConfigName(std::wstring const& configName)
    {
        std::wstring cleaned{};

        for (auto const ch : configName)
        {
            if (ch >= 0x20)
            {
                cleaned += ch;
            }
        }

        while (!cleaned.empty() && std::iswspace(cleaned.front()))
        {
            cleaned.erase(cleaned.begin());
        }

        while (!cleaned.empty() && std::iswspace(cleaned.back()))
        {
            cleaned.pop_back();
        }

        if (cleaned.size() > MaximumConfigNameLength)
        {
            cleaned.resize(MaximumConfigNameLength);
        }

        return cleaned;
    }

    _Use_decl_annotations_
    std::vector<ConfigFileEntry> ListConfigurations(ConfigEnvironment const& environment) noexcept
    {
        std::vector<ConfigFileEntry> files{};

        try
        {
            if (environment.ConfigFolder.empty())
            {
                return files;
            }

            auto const current = environment.ReadCurrentFileName ? environment.ReadCurrentFileName() : std::wstring{};

            WIN32_FIND_DATAW found{};

            auto const pattern = environment.ConfigFolder + L"*" + std::wstring{ ConfigFileSuffix };
            auto const find = ::FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

            if (find == INVALID_HANDLE_VALUE)
            {
                return files;
            }

            do
            {
                if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
                {
                    continue;
                }

                ConfigFileEntry entry{};
                entry.FileName = found.cFileName;

                // a short name can match the pattern when the long one doesn't
                if (!IsConfigFileName(entry.FileName))
                {
                    continue;
                }

                entry.LastWritten = found.ftLastWriteTime;
                entry.Created = found.ftCreationTime;
                entry.IsCurrent = SameText(entry.FileName, current);

                std::string bytes{};

                if (ReadWholeFile(environment.ConfigFolder + entry.FileName, MaximumConfigBytes, bytes).Succeeded())
                {
                    entry.Inventory = TakeInventory(std::string_view{ bytes });
                    entry.Readable = entry.Inventory.IsConfiguration;
                    entry.ConfigName = entry.Inventory.ConfigName;
                }

                if (entry.ConfigName.empty())
                {
                    entry.ConfigName = StemOf(entry.FileName);
                }

                files.push_back(std::move(entry));
            }
            while (::FindNextFileW(find, &found));

            ::FindClose(find);

            std::sort(files.begin(), files.end(), [](auto const& left, auto const& right)
                {
                    return ::CompareStringEx(
                        LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS,
                        left.ConfigName.c_str(), static_cast<int>(left.ConfigName.size()),
                        right.ConfigName.c_str(), static_cast<int>(right.ConfigName.size()),
                        nullptr, nullptr, 0) == CSTR_LESS_THAN;
                });
        }
        catch (...)
        {
            files.clear();
        }

        return files;
    }

    _Use_decl_annotations_
    std::wstring FileNameForNewConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& configName) noexcept
    {
        try
        {
            std::wstring stem{};

            // The same rule the app has always used: characters that are safe in a file name and
            // free of the json and shell punctuation that has caused trouble before.
            for (auto const ch : configName)
            {
                if (std::iswalnum(ch) || ch == L' ' || ch == L'-' || ch == L'_' || ch == L'(' || ch == L')')
                {
                    stem += ch;
                }
            }

            while (!stem.empty() && stem.front() == L' ')
            {
                stem.erase(stem.begin());
            }

            while (!stem.empty() && stem.back() == L' ')
            {
                stem.pop_back();
            }

            if (stem.size() > MaximumConfigNameLength)
            {
                stem.resize(MaximumConfigNameLength);
            }

            if (stem.empty())
            {
                stem = L"Configuration";
            }

            for (uint32_t attempt = 1; attempt <= MaximumNameAttempts; attempt++)
            {
                auto const fileName = (attempt == 1 ? stem : stem + L" " + std::to_wstring(attempt)) + std::wstring{ ConfigFileSuffix };

                if (!FileExists(environment.ConfigFolder + fileName))
                {
                    return fileName;
                }
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    CreateConfigResult CreateConfigurationFromBytes(
        ConfigEnvironment const& environment,
        std::wstring const& configName,
        std::string const& configurationBytes) noexcept
    {
        CreateConfigResult result{};

        try
        {
            auto const name = CleanConfigName(configName);

            if (name.empty())
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            auto const document = jsonutil::ParseObject(configurationBytes);

            if (document == nullptr || jsonutil::ObjectValue(document, L"endpointTransportPluginSettings") == nullptr)
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            return WriteNewConfiguration(environment, name, jsonutil::Serialize(WithConfigName(document, name)));
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    CreateConfigResult CreateConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& configName,
        std::wstring const& copyOfFileName) noexcept
    {
        CreateConfigResult result{};

        try
        {
            auto const name = CleanConfigName(configName);

            if (name.empty())
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            if (copyOfFileName.empty())
            {
                return WriteNewConfiguration(environment, name, EmptyConfiguration(name));
            }

            if (!IsConfigFileName(copyOfFileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            std::string bytes{};

            auto const read = ReadWholeFile(environment.ConfigFolder + copyOfFileName, MaximumConfigBytes, bytes);

            if (!read.Succeeded())
            {
                result.Error = read.Error;
                result.SystemError = read.SystemError;
                return result;
            }

            return CreateConfigurationFromBytes(environment, name, bytes);
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult RenameConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& fileName,
        std::wstring const& configName) noexcept
    {
        ConfigResult result{};

        try
        {
            auto const name = CleanConfigName(configName);

            if (name.empty() || !IsConfigFileName(fileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            auto const path = environment.ConfigFolder + fileName;

            std::string bytes{};

            auto const read = ReadWholeFile(path, MaximumConfigBytes, bytes);

            if (!read.Succeeded())
            {
                return read;
            }

            auto const document = jsonutil::ParseObject(bytes);

            if (document == nullptr || jsonutil::ObjectValue(document, L"endpointTransportPluginSettings") == nullptr)
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            auto const renamed = jsonutil::Serialize(WithConfigName(document, name));

            if (renamed.empty())
            {
                result.Error = ConfigError::Unexpected;
                return result;
            }

            auto const written = WriteInPlace(path, renamed, &bytes);

            result.Error = written.Error;
            result.SystemError = written.SystemError;

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult SwitchConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& fileName) noexcept
    {
        ConfigResult result{};

        try
        {
            if (!IsConfigFileName(fileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            if (!FileExists(environment.ConfigFolder + fileName))
            {
                result.Error = ConfigError::NotFound;
                return result;
            }

            if (!environment.WriteCurrentFileName || !environment.WriteCurrentFileName(fileName))
            {
                result.Error = ConfigError::RegistryFailed;
            }

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult DeleteConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& fileName) noexcept
    {
        ConfigResult result{};

        try
        {
            if (!IsConfigFileName(fileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            auto const current = environment.ReadCurrentFileName ? environment.ReadCurrentFileName() : std::wstring{};

            if (SameText(fileName, current))
            {
                result.Error = ConfigError::InUse;
                return result;
            }

            auto const backup = MakeBackup(environment, fileName, BackupReason::BeforeDelete, {}, {});

            if (!backup.Succeeded())
            {
                result.Error = backup.Error == ConfigError::NotFound ? ConfigError::NotFound : ConfigError::BackupFailed;
                result.SystemError = backup.SystemError;
                return result;
            }

            return DeleteFileAt(environment.ConfigFolder + fileName);
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }
}
