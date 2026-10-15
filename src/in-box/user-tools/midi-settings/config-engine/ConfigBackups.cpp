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

#include "ConfigBackups.h"
#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigPictures.h"
#include "ConfigWrite.h"

#include <algorithm>
#include <map>

namespace midisettings::configengine
{
    namespace
    {
        // Backups are named to the second, so this only matters for repeated clicks.
        constexpr uint32_t MaximumBackupsPerSecond{ 100 };

        // A file name longer than this would push the date off the end in Explorer.
        constexpr size_t MaximumBackupNameStem{ 80 };

        constexpr uint64_t FileTimeTicksPerSecond{ 10'000'000ull };

        // How long a change may take after the backup made for it, for the window to say what the
        // change was. An import or a restore writes within seconds of its backup.
        constexpr uint64_t ChangeAfterBackupWindow{ 10ull * 60 * FileTimeTicksPerSecond };

        std::wstring StemOf(_In_ std::wstring const& configFileName)
        {
            if (EndsWithText(configFileName, ConfigFileSuffix))
            {
                return configFileName.substr(0, configFileName.size() - ConfigFileSuffix.size());
            }

            return configFileName;
        }

        // The configuration's name, made safe for a file name.
        std::wstring BackupNameStem(_In_ std::wstring const& configName, _In_ std::wstring const& configFileName)
        {
            std::wstring stem{};

            for (auto const ch : configName.empty() ? StemOf(configFileName) : configName)
            {
                if (ch < 0x20 || ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' ||
                    ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
                {
                    continue;
                }

                stem += ch;
            }

            if (stem.size() > MaximumBackupNameStem)
            {
                stem.resize(MaximumBackupNameStem);
            }

            while (!stem.empty() && (stem.back() == L' ' || stem.back() == L'.'))
            {
                stem.pop_back();
            }

            while (!stem.empty() && (stem.front() == L' ' || stem.front() == L'.'))
            {
                stem.erase(stem.begin());
            }

            return stem.empty() ? std::wstring{ L"Configuration" } : stem;
        }

        bool EnsureFolder(_In_ std::wstring const& folder) noexcept
        {
            if (folder.empty())
            {
                return false;
            }

            if (::CreateDirectoryW(folder.c_str(), nullptr))
            {
                return true;
            }

            auto const attributes = ::GetFileAttributesW(folder.c_str());

            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        }

        std::wstring ConfigNameFromBytes(_In_ std::string_view const bytes)
        {
            auto const document = jsonutil::ParseObject(bytes);

            return document == nullptr ? std::wstring{} : ConfigNameOf(document);
        }

        bool IsUsableConfigFileName(_In_ std::wstring const& configFileName) noexcept
        {
            return IsUsableFileName(configFileName) &&
                configFileName.size() > ConfigFileSuffix.size() &&
                EndsWithText(configFileName, ConfigFileSuffix);
        }
    }

    _Use_decl_annotations_
    bool TryGetBackedUpFileName(
        std::wstring_view const backupFileName,
        std::wstring& configFileName,
        BackupKind& kind)
    {
        configFileName.clear();
        kind = BackupKind::Daily;

        if (!EndsWithText(backupFileName, BackupFileSuffix))
        {
            return false;
        }

        auto const withoutSuffix = backupFileName.substr(0, backupFileName.size() - BackupFileSuffix.size());
        auto const lastDot = withoutSuffix.find_last_of(L'.');

        if (lastDot == std::wstring_view::npos || lastDot + 1 == withoutSuffix.size())
        {
            return false;
        }

        auto const stamp = withoutSuffix.substr(lastDot + 1);

        // digits and dashes: the API's daily 2026-10-14, or an earlier MIDI Settings' 2026-10-14-153012
        if (stamp.find_first_not_of(L"0123456789-") != std::wstring_view::npos)
        {
            return false;
        }

        auto const original = withoutSuffix.substr(0, lastDot);

        if (!EndsWithText(original, ConfigFileSuffix) || !IsUsableFileName(original))
        {
            return false;
        }

        kind = stamp.size() == 10 ? BackupKind::Daily : BackupKind::Earlier;
        configFileName = std::wstring{ original };

        return true;
    }

    _Use_decl_annotations_
    std::vector<BackupEntry> ListBackups(ConfigEnvironment const& environment) noexcept
    {
        std::vector<BackupEntry> backups{};

        try
        {
            WIN32_FIND_DATAW found{};

            if (!environment.BackupFolder.empty())
            {
                auto const pattern = environment.BackupFolder + L"*" + std::wstring{ PackageFileSuffix };

                auto const find = ::FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

                if (find != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
                        {
                            continue;
                        }

                        BackupEntry backup{};
                        backup.FileName = found.cFileName;
                        backup.Path = environment.BackupFolder + backup.FileName;
                        backup.Kind = BackupKind::Full;

                        PackageInfo info{};

                        if (!ReadPackageInfo(backup.Path, info).Succeeded())
                        {
                            continue;
                        }

                        backup.ConfigName = info.ConfigName;
                        backup.ConfigFileName = info.ConfigFileName;
                        backup.MadeAt = info.HasCreated ? info.Created : found.ftCreationTime;
                        backup.Reason = info.Reason;
                        backup.ReasonComputerName = info.ReasonComputerName;
                        backup.ReasonConfigName = info.ReasonConfigName;

                        backups.push_back(std::move(backup));
                    }
                    while (::FindNextFileW(find, &found));

                    ::FindClose(find);
                }
            }

            if (!environment.ConfigFolder.empty())
            {
                auto const pattern = environment.ConfigFolder + L"*" + std::wstring{ ConfigFileSuffix } + L".*" + std::wstring{ BackupFileSuffix };

                auto const find = ::FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

                if (find != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        if ((found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
                        {
                            continue;
                        }

                        BackupEntry backup{};
                        backup.FileName = found.cFileName;
                        backup.Path = environment.ConfigFolder + backup.FileName;

                        if (!TryGetBackedUpFileName(backup.FileName, backup.ConfigFileName, backup.Kind))
                        {
                            continue;
                        }

                        backup.MadeAt = found.ftLastWriteTime;

                        std::string bytes{};

                        if (ReadWholeFile(backup.Path, MaximumConfigBytes, bytes).Succeeded())
                        {
                            backup.ConfigName = ConfigNameFromBytes(bytes);
                        }

                        if (backup.ConfigName.empty())
                        {
                            backup.ConfigName = StemOf(backup.ConfigFileName);
                        }

                        backups.push_back(std::move(backup));
                    }
                    while (::FindNextFileW(find, &found));

                    ::FindClose(find);
                }
            }

            std::sort(backups.begin(), backups.end(), [](auto const& left, auto const& right)
                {
                    return ::CompareFileTime(&left.MadeAt, &right.MadeAt) > 0;
                });
        }
        catch (...)
        {
            backups.clear();
        }

        return backups;
    }

    _Use_decl_annotations_
    BackupResult MakeBackupOfBytes(
        ConfigEnvironment const& environment,
        std::wstring const& configFileName,
        std::string const& configurationBytes,
        BackupReason const reason,
        std::wstring const& reasonComputerName,
        std::wstring const& reasonConfigName) noexcept
    {
        BackupResult result{};

        try
        {
            if (!IsUsableConfigFileName(configFileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            if (!LooksLikeConfiguration(configurationBytes))
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            if (!EnsureFolder(environment.BackupFolder))
            {
                result.Error = ConfigError::NoFolder;
                result.SystemError = ::GetLastError();
                return result;
            }

            auto const inventory = TakeInventory(std::string_view{ configurationBytes });

            PackageSource source{};
            source.Info.Kind = PackageKind::Backup;
            source.Info.ConfigName = inventory.ConfigName.empty() ? StemOf(configFileName) : inventory.ConfigName;
            source.Info.ConfigFileName = configFileName;
            source.Info.ComputerName = environment.ComputerName;
            source.Info.Created = CurrentFileTime();
            source.Info.HasCreated = true;
            source.Info.Reason = reason;
            source.Info.ReasonComputerName = reasonComputerName;
            source.Info.ReasonConfigName = reasonConfigName;
            source.Info.AppVersion = environment.AppVersion;
            source.Info.Parts = PartBit(ConfigPart::All);
            source.ConfigurationBytes = configurationBytes;
            source.PortNamingValue = environment.ReadPortNaming ? environment.ReadPortNaming() : PortNaming::None;
            source.Pictures = PictureFiles(environment, inventory.Pictures);

            SYSTEMTIME local{};
            FILETIME localTime{};

            if (!::FileTimeToLocalFileTime(&source.Info.Created, &localTime) || !::FileTimeToSystemTime(&localTime, &local))
            {
                ::GetLocalTime(&local);
            }

            wchar_t stamp[32]{};

            ::swprintf_s(stamp, L"%04u-%02u-%02u %02u%02u%02u",
                static_cast<unsigned>(local.wYear), static_cast<unsigned>(local.wMonth), static_cast<unsigned>(local.wDay),
                static_cast<unsigned>(local.wHour), static_cast<unsigned>(local.wMinute), static_cast<unsigned>(local.wSecond));

            auto const stem = BackupNameStem(source.Info.ConfigName, configFileName) + L" " + stamp;

            for (uint32_t attempt = 1; attempt <= MaximumBackupsPerSecond; attempt++)
            {
                auto const fileName = stem + (attempt > 1 ? L"-" + std::to_wstring(attempt) : std::wstring{}) + std::wstring{ PackageFileSuffix };
                auto const path = environment.BackupFolder + fileName;

                // The name is taken first, so a backup never lands on top of another file.
                auto const reserved = WriteNewFile(path, std::string{});

                if (reserved.Error == ConfigError::NameTaken)
                {
                    continue;
                }

                if (!reserved.Succeeded())
                {
                    result.Error = ConfigError::BackupFailed;
                    result.SystemError = reserved.SystemError;
                    return result;
                }

                auto const written = WritePackage(path, source);

                if (!written.Succeeded())
                {
                    DeleteFileAt(path);

                    result.Error = ConfigError::BackupFailed;
                    result.SystemError = written.SystemError;
                    return result;
                }

                result.Backup.Path = path;
                result.Backup.FileName = fileName;
                result.Backup.Kind = BackupKind::Full;
                result.Backup.ConfigFileName = configFileName;
                result.Backup.ConfigName = source.Info.ConfigName;
                result.Backup.MadeAt = source.Info.Created;
                result.Backup.Reason = reason;
                result.Backup.ReasonComputerName = reasonComputerName;
                result.Backup.ReasonConfigName = reasonConfigName;

                return result;
            }

            result.Error = ConfigError::BackupFailed;
            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    BackupResult MakeBackup(
        ConfigEnvironment const& environment,
        std::wstring const& configFileName,
        BackupReason const reason,
        std::wstring const& reasonComputerName,
        std::wstring const& reasonConfigName) noexcept
    {
        BackupResult result{};

        try
        {
            if (!IsUsableConfigFileName(configFileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            std::string bytes{};

            auto const read = ReadWholeFile(environment.ConfigFolder + configFileName, MaximumConfigBytes, bytes);

            if (!read.Succeeded())
            {
                result.Error = read.Error;
                result.SystemError = read.SystemError;
                return result;
            }

            if (bytes.empty())
            {
                result.Error = ConfigError::Empty;
                return result;
            }

            return MakeBackupOfBytes(environment, configFileName, bytes, reason, reasonComputerName, reasonConfigName);
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    RestoreResult RestoreBackup(
        ConfigEnvironment const& environment,
        BackupEntry const& backup,
        bool const backUpFirst) noexcept
    {
        RestoreResult result{};

        try
        {
            result.ConfigFileName = backup.ConfigFileName;

            if (!IsUsableConfigFileName(backup.ConfigFileName))
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            PackageContents contents{};
            std::string bytes{};

            if (backup.Kind == BackupKind::Full)
            {
                auto const read = ReadPackage(backup.Path, contents);

                if (!read.Succeeded())
                {
                    result.Error = read.Error;
                    result.SystemError = read.SystemError;
                    return result;
                }

                bytes = contents.ConfigurationBytes;
            }
            else
            {
                auto const read = ReadWholeFile(backup.Path, MaximumConfigBytes, bytes);

                if (!read.Succeeded())
                {
                    result.Error = read.Error;
                    result.SystemError = read.SystemError;
                    return result;
                }

                if (!LooksLikeConfiguration(bytes))
                {
                    result.Error = ConfigError::NotAConfiguration;
                    return result;
                }
            }

            auto const path = environment.ConfigFolder + backup.ConfigFileName;

            // What's there now, which is backed up before anything changes. A file that can't be
            // read can't be backed up either, so nothing is written over it.
            std::string current{};

            auto const readCurrent = ReadWholeFile(path, MaximumConfigBytes, current);

            if (!readCurrent.Succeeded() && readCurrent.Error != ConfigError::NotFound)
            {
                result.Error = ConfigError::BackupFailed;
                result.SystemError = readCurrent.SystemError;
                return result;
            }

            if (backUpFirst && !current.empty() && LooksLikeConfiguration(current))
            {
                auto safety = MakeBackupOfBytes(environment, backup.ConfigFileName, current, BackupReason::BeforeRestore, {}, {});

                if (!safety.Succeeded())
                {
                    result.Error = ConfigError::BackupFailed;
                    result.SystemError = safety.SystemError;
                    return result;
                }

                result.SafetyBackup = std::move(safety.Backup);
            }

            if (backup.Kind == BackupKind::Full && !contents.Pictures.empty())
            {
                auto const inventory = TakeInventory(std::string_view{ bytes });
                auto const stored = StorePictures(environment, contents, inventory.Pictures);

                result.PicturesStored = stored.Stored;
                result.PicturesFailed = stored.Failed;

                auto const renamed = std::any_of(stored.Renames.begin(), stored.Renames.end(),
                    [](auto const& rename) { return rename.first != rename.second; });

                if (renamed)
                {
                    auto const document = jsonutil::ParseObject(bytes);

                    if (document != nullptr)
                    {
                        RenamePictures(document, stored.Renames);

                        auto rewritten = jsonutil::Serialize(document);

                        if (!rewritten.empty())
                        {
                            bytes = std::move(rewritten);
                        }
                    }
                }
            }

            auto const written = WriteInPlace(path, bytes, &current);

            if (!written.Succeeded())
            {
                result.Error = written.Error;
                result.SystemError = written.SystemError;
                return result;
            }

            result.Unchanged = written.Unchanged;

            if (backup.Kind == BackupKind::Full && contents.PortNamingValue != PortNaming::None &&
                environment.ReadPortNaming && environment.WritePortNaming)
            {
                auto const usable = contents.PortNamingValue != PortNaming::Automatic || environment.AutomaticPortNamingAvailable;

                if (usable && environment.ReadPortNaming() != contents.PortNamingValue)
                {
                    if (!environment.WritePortNaming(contents.PortNamingValue))
                    {
                        result.Error = ConfigError::RegistryFailed;
                        return result;
                    }

                    result.PortNamingChanged = true;
                }
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
    ConfigResult DeleteBackup(BackupEntry const& backup) noexcept
    {
        try
        {
            return DeleteFileAt(backup.Path);
        }
        catch (...)
        {
            ConfigResult result{};
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    std::vector<BackupEntry> OldBackups(
        std::vector<BackupEntry> const& backups,
        FILETIME const& now,
        uint32_t const keepDays,
        uint32_t const keepNewestOfEach) noexcept
    {
        std::vector<BackupEntry> old{};

        try
        {
            auto const window = static_cast<uint64_t>(keepDays) * 24ull * 60ull * 60ull * FileTimeTicksPerSecond;
            auto const nowValue = FileTimeValue(now);
            auto const cutoff = nowValue > window ? nowValue - window : 0;

            // newest first within each configuration
            std::vector<BackupEntry> sorted{ backups };

            std::sort(sorted.begin(), sorted.end(), [](auto const& left, auto const& right)
                {
                    return ::CompareFileTime(&left.MadeAt, &right.MadeAt) > 0;
                });

            std::map<std::wstring, uint32_t> keptSoFar{};

            for (auto const& backup : sorted)
            {
                auto key = backup.ConfigFileName;

                if (!key.empty())
                {
                    ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, backup.ConfigFileName.c_str(),
                        static_cast<int>(backup.ConfigFileName.size()), key.data(), static_cast<int>(key.size()), nullptr, nullptr, 0);
                }

                auto& kept = keptSoFar[key];

                if (kept < keepNewestOfEach)
                {
                    kept++;
                    continue;
                }

                if (FileTimeValue(backup.MadeAt) < cutoff)
                {
                    old.push_back(backup);
                }
            }
        }
        catch (...)
        {
            old.clear();
        }

        return old;
    }

    _Use_decl_annotations_
    DeleteBackupsResult DeleteBackups(std::vector<BackupEntry> const& backups) noexcept
    {
        DeleteBackupsResult result{};

        try
        {
            for (auto const& backup : backups)
            {
                auto const deleted = DeleteFileAt(backup.Path);

                if (deleted.Succeeded())
                {
                    result.Deleted++;
                }
                else
                {
                    result.Failed++;
                    result.Error = deleted.Error;
                    result.SystemError = deleted.SystemError;
                }
            }
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
        }

        return result;
    }

    _Use_decl_annotations_
    std::optional<BackupEntry> BackupBeforeLastChange(
        std::wstring const& configFileName,
        FILETIME const& lastWritten,
        std::vector<BackupEntry> const& backups) noexcept
    {
        try
        {
            auto const written = FileTimeValue(lastWritten);

            // a little slack, because the backup's time is read from the clock and the file's from
            // the file system
            auto const latest = written + 2 * FileTimeTicksPerSecond;

            BackupEntry const* newest{ nullptr };

            for (auto const& backup : backups)
            {
                if (backup.Kind != BackupKind::Full || !SameText(backup.ConfigFileName, configFileName))
                {
                    continue;
                }

                auto const made = FileTimeValue(backup.MadeAt);

                if (made > latest)
                {
                    continue;
                }

                if (newest == nullptr || made > FileTimeValue(newest->MadeAt))
                {
                    newest = &backup;
                }
            }

            if (newest == nullptr ||
                (newest->Reason != BackupReason::BeforeImport && newest->Reason != BackupReason::BeforeRestore))
            {
                return std::nullopt;
            }

            auto const made = FileTimeValue(newest->MadeAt);

            if (written > made && written - made > ChangeAfterBackupWindow)
            {
                return std::nullopt;
            }

            return *newest;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}
