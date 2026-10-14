// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// IN-BOX MICROSOFT TOOL ONLY - DO NOT COPY THIS APPROACH
//
// This file backs up, adds and restores Windows MIDI Services configuration files directly. That
// is supported only for the MIDI tools that ship in Windows, of which this is one. The file name,
// the folder it lives in and the JSON schema inside the file are all implementation details and
// can change in any release without notice.
//
// Applications and third-party tools must never open, parse, edit, merge, back up or restore
// that file, for any reason. Use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#include "pch.h"
#include "MidiConfig.h"

#include "json_defs.h"

namespace midisettings::config
{
    namespace
    {
        constexpr std::wstring_view ConfigFileSuffix{ L".midiconfig.json" };
        constexpr std::wstring_view BackupFileSuffix{ L".bak" };

        // Far larger than any real configuration, and still small enough to read in one go.
        constexpr uint64_t MaximumFileBytes{ 16 * 1024 * 1024 };

        // The same patience the API's writer has with a file another program has open.
        constexpr uint32_t SharingRetryCount{ 20 };
        constexpr DWORD SharingRetryWaitMilliseconds{ 100 };

        // Backups are named to the second, so this only matters for repeated clicks.
        constexpr uint32_t MaximumBackupsPerSecond{ 100 };

        bool EndsWithInsensitive(_In_ std::wstring_view const text, _In_ std::wstring_view const suffix) noexcept
        {
            return text.size() > suffix.size() &&
                ::CompareStringOrdinal(
                    text.data() + (text.size() - suffix.size()),
                    static_cast<int>(suffix.size()),
                    suffix.data(),
                    static_cast<int>(suffix.size()),
                    TRUE) == CSTR_EQUAL;
        }

        // A configuration is only ever a bare name in the one folder the service reads.
        bool IsUsableFileName(_In_ std::wstring_view const name) noexcept
        {
            return !name.empty() &&
                name.size() < MAX_PATH &&
                name.find_first_of(LR"(\/:*?"<>|)") == std::wstring_view::npos &&
                name.find_first_not_of(L'.') != std::wstring_view::npos;
        }

        // <configuration file name>.<stamp>.bak, where the stamp is digits and dashes. That
        // covers the API's daily 2026-10-14 form and this app's 2026-10-14-153012 form.
        bool TryGetBackedUpFileName(_In_ std::wstring_view const backupFileName, _Out_ std::wstring& configFileName)
        {
            configFileName.clear();

            if (!EndsWithInsensitive(backupFileName, BackupFileSuffix))
            {
                return false;
            }

            auto const withoutSuffix = backupFileName.substr(0, backupFileName.size() - BackupFileSuffix.size());
            auto const lastDot = withoutSuffix.find_last_of(L'.');

            if (lastDot == std::wstring_view::npos || lastDot + 1 == withoutSuffix.size())
            {
                return false;
            }

            if (withoutSuffix.substr(lastDot + 1).find_first_not_of(L"0123456789-") != std::wstring_view::npos)
            {
                return false;
            }

            auto const original = withoutSuffix.substr(0, lastDot);

            if (!EndsWithInsensitive(original, ConfigFileSuffix) || !IsUsableFileName(original))
            {
                return false;
            }

            configFileName = std::wstring{ original };

            return true;
        }

        // The share mode lets no one else write while the file is open here, so a read never
        // sees half of a save. The API's writer waits its turn the same way this does.
        HANDLE OpenWithRetry(
            _In_ std::wstring const& path,
            _In_ DWORD const access,
            _In_ DWORD const disposition,
            _In_ DWORD const flags,
            _Out_ DWORD& systemError) noexcept
        {
            systemError = ERROR_SUCCESS;

            for (uint32_t attempt = 0; attempt < SharingRetryCount; attempt++)
            {
                auto const file = ::CreateFileW(
                    path.c_str(),
                    access,
                    FILE_SHARE_READ,
                    nullptr,
                    disposition,
                    FILE_ATTRIBUTE_NORMAL | flags,
                    nullptr);

                if (file != INVALID_HANDLE_VALUE)
                {
                    systemError = ERROR_SUCCESS;
                    return file;
                }

                systemError = ::GetLastError();

                if (systemError != ERROR_SHARING_VIOLATION && systemError != ERROR_LOCK_VIOLATION)
                {
                    break;
                }

                ::Sleep(SharingRetryWaitMilliseconds);
            }

            return INVALID_HANDLE_VALUE;
        }

        ConfigFileError ReadFromHandle(
            _In_ HANDLE const file,
            _Out_ std::string& bytes,
            _Out_ DWORD& systemError)
        {
            bytes.clear();
            systemError = ERROR_SUCCESS;

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file, &size))
            {
                systemError = ::GetLastError();
                return ConfigFileError::ReadFailed;
            }

            if (size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > MaximumFileBytes)
            {
                return ConfigFileError::TooLarge;
            }

            bytes.resize(static_cast<size_t>(size.QuadPart));

            size_t total{ 0 };

            while (total < bytes.size())
            {
                DWORD read{ 0 };

                if (!::ReadFile(file, bytes.data() + total, static_cast<DWORD>(bytes.size() - total), &read, nullptr))
                {
                    systemError = ::GetLastError();
                    bytes.clear();
                    return ConfigFileError::ReadFailed;
                }

                if (read == 0)
                {
                    break;
                }

                total += read;
            }

            bytes.resize(total);

            return ConfigFileError::None;
        }

        ConfigFileError ReadWholeFile(
            _In_ std::wstring const& path,
            _Out_ std::string& bytes,
            _Out_ DWORD& systemError)
        {
            bytes.clear();

            wil::unique_hfile file{ OpenWithRetry(path, GENERIC_READ, OPEN_EXISTING, 0, systemError) };

            if (!file)
            {
                return systemError == ERROR_FILE_NOT_FOUND || systemError == ERROR_PATH_NOT_FOUND ?
                    ConfigFileError::NotFound :
                    ConfigFileError::ReadFailed;
            }

            return ReadFromHandle(file.get(), bytes, systemError);
        }

        // In place, never through a temporary file and a rename: the folder lets people write
        // files there but not always delete them. That is also how the API saves.
        bool WriteToHandle(
            _In_ HANDLE const file,
            _In_ std::string const& bytes,
            _Out_ DWORD& systemError) noexcept
        {
            systemError = ERROR_SUCCESS;

            LARGE_INTEGER const start{};
            DWORD written{ 0 };

            // The old contents stay unless they are cut off after the new end
            if (!::SetFilePointerEx(file, start, nullptr, FILE_BEGIN) ||
                !::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
                written != bytes.size() ||
                !::SetEndOfFile(file))
            {
                systemError = ::GetLastError();
                return false;
            }

            return true;
        }

        // Enough to know the file is a Windows MIDI Services configuration rather than some
        // other JSON, without judging what the service will make of the entries in it.
        bool LooksLikeConfiguration(_In_ std::string const& bytes)
        {
            std::string_view text{ bytes };

            if (text.size() >= 3 &&
                static_cast<uint8_t>(text[0]) == 0xEF &&
                static_cast<uint8_t>(text[1]) == 0xBB &&
                static_cast<uint8_t>(text[2]) == 0xBF)
            {
                text.remove_prefix(3);
            }

            json::JsonObject document{ nullptr };

            if (!json::JsonObject::TryParse(winrt::to_hstring(text), document) || document == nullptr)
            {
                return false;
            }

            auto const settings = document.TryLookup(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

            return settings != nullptr && settings.ValueType() == json::JsonValueType::Object;
        }

        // <name>.<yyyy-MM-dd-HHmmss>.bak in local time, like the API's daily backups, with a
        // count on the end when two land in the same second.
        std::wstring BackupFileNameFor(
            _In_ std::wstring const& configFileName,
            _In_ SYSTEMTIME const& now,
            _In_ uint32_t const attempt)
        {
            auto stamp = std::format(
                L"{:04}-{:02}-{:02}-{:02}{:02}{:02}",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

            if (attempt > 0)
            {
                stamp += std::format(L"-{}", attempt + 1);
            }

            return configFileName + L"." + stamp + std::wstring{ BackupFileSuffix };
        }

        // Old backups are never removed, for the same reasons the API keeps its own: the folder
        // does not always allow it, and losing a backup is worse than keeping one.
        ConfigFileOperationResult WriteBackup(
            _In_ std::wstring const& folder,
            _In_ std::wstring const& configFileName,
            _In_ std::string const& bytes)
        {
            ConfigFileOperationResult result{};
            result.ConfigFileName = configFileName;

            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            for (uint32_t attempt = 0; attempt < MaximumBackupsPerSecond; attempt++)
            {
                auto const backupFileName = BackupFileNameFor(configFileName, now, attempt);

                DWORD systemError{ ERROR_SUCCESS };

                wil::unique_hfile file{ OpenWithRetry(folder + backupFileName, GENERIC_WRITE, CREATE_NEW, 0, systemError) };

                if (!file)
                {
                    if (systemError == ERROR_FILE_EXISTS || systemError == ERROR_ALREADY_EXISTS)
                    {
                        continue;
                    }

                    result.Error = ConfigFileError::BackupFailed;
                    result.SystemError = systemError;
                    return result;
                }

                if (!WriteToHandle(file.get(), bytes, systemError))
                {
                    result.Error = ConfigFileError::BackupFailed;
                    result.SystemError = systemError;
                    return result;
                }

                result.BackupFileName = backupFileName;
                return result;
            }

            result.Error = ConfigFileError::BackupFailed;
            return result;
        }

        // Writes a configuration into the folder under its own name. Whatever is there under
        // that name is backed up first, and when that backup cannot be made nothing changes.
        ConfigFileOperationResult PutInPlace(
            _In_ std::wstring const& configFileName,
            _In_ std::string const& bytes)
        {
            ConfigFileOperationResult result{};
            result.ConfigFileName = configFileName;

            auto const folder = FolderPath();

            if (folder.empty())
            {
                result.Error = ConfigFileError::NoFolder;
                return result;
            }

            DWORD systemError{ ERROR_SUCCESS };

            // Held open from the read to the write, so nothing can save in between and then be
            // lost without a backup.
            wil::unique_hfile file{ OpenWithRetry(
                folder + configFileName,
                GENERIC_READ | GENERIC_WRITE,
                OPEN_ALWAYS,
                FILE_FLAG_OPEN_REPARSE_POINT,
                systemError) };

            if (!file)
            {
                result.Error = ConfigFileError::WriteFailed;
                result.SystemError = systemError;
                return result;
            }

            // Anyone can add files to this folder, and this runs as an administrator, so a link
            // planted under a configuration's name must not send the write somewhere else.
            BY_HANDLE_FILE_INFORMATION information{};

            if (!::GetFileInformationByHandle(file.get(), &information) ||
                (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                information.nNumberOfLinks != 1)
            {
                result.Error = ConfigFileError::WriteFailed;
                result.SystemError = ERROR_CANT_ACCESS_FILE;
                return result;
            }

            std::string existing{};

            if (ReadFromHandle(file.get(), existing, systemError) != ConfigFileError::None)
            {
                // What is there could not be read, so it could not be backed up either
                result.Error = ConfigFileError::BackupFailed;
                result.SystemError = systemError;
                return result;
            }

            if (existing == bytes)
            {
                result.Unchanged = true;
                return result;
            }

            if (!existing.empty())
            {
                auto backup = WriteBackup(folder, configFileName, existing);

                if (backup.Error != ConfigFileError::None)
                {
                    return backup;
                }

                result.BackupFileName = std::move(backup.BackupFileName);
            }

            if (!WriteToHandle(file.get(), bytes, systemError))
            {
                result.Error = ConfigFileError::WriteFailed;
                result.SystemError = systemError;
            }

            return result;
        }

        // A backup goes back under the name of the configuration it came from. Anything else
        // keeps its own name, with the suffix the folder listing looks for.
        std::wstring ConfigFileNameForSource(_In_ std::wstring const& sourceFileName)
        {
            std::wstring configFileName{};

            if (TryGetBackedUpFileName(sourceFileName, configFileName))
            {
                return configFileName;
            }

            if (EndsWithInsensitive(sourceFileName, ConfigFileSuffix))
            {
                return sourceFileName;
            }

            auto stem = sourceFileName;
            auto const lastDot = stem.find_last_of(L'.');

            if (lastDot != std::wstring::npos && lastDot > 0)
            {
                stem.erase(lastDot);
            }

            return stem + std::wstring{ ConfigFileSuffix };
        }
    }

    _Use_decl_annotations_
    std::wstring SystemErrorText(DWORD const error) noexcept
    {
        if (error == ERROR_SUCCESS)
        {
            return {};
        }

        try
        {
            wil::unique_hlocal_string buffer;

            auto const characters = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                0,
                reinterpret_cast<LPWSTR>(buffer.put()),
                0,
                nullptr);

            if (characters > 0 && buffer)
            {
                std::wstring message{ buffer.get() };

                while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
                {
                    message.pop_back();
                }

                return message;
            }
        }
        catch (...)
        {
        }

        return {};
    }

    std::vector<ConfigBackupInfo> EnumerateBackups() noexcept
    {
        std::vector<ConfigBackupInfo> backups{};

        try
        {
            auto const folder = FolderPath();

            if (folder.empty())
            {
                return backups;
            }

            auto const pattern = folder + L"*" + std::wstring{ ConfigFileSuffix } + L".*" + std::wstring{ BackupFileSuffix };

            WIN32_FIND_DATAW findData{};

            wil::unique_hfind find{ ::FindFirstFileExW(
                pattern.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0) };

            if (!find)
            {
                return backups;
            }

            do
            {
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    continue;
                }

                ConfigBackupInfo backup{};
                backup.BackupFileName = findData.cFileName;

                if (!TryGetBackedUpFileName(backup.BackupFileName, backup.ConfigFileName))
                {
                    continue;
                }

                backup.ConfigName = ReadConfigName(folder + backup.BackupFileName);
                backup.WrittenAt = findData.ftLastWriteTime;

                backups.push_back(std::move(backup));
            }
            while (::FindNextFileW(find.get(), &findData));

            std::sort(backups.begin(), backups.end(), [](auto const& left, auto const& right)
                {
                    return ::CompareFileTime(&left.WrittenAt, &right.WrittenAt) > 0;
                });
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to list the configuration backups.")

        return backups;
    }

    _Use_decl_annotations_
    ConfigFileOperationResult BackUpFile(std::wstring const& configFileName) noexcept
    {
        ConfigFileOperationResult result{};

        try
        {
            result.ConfigFileName = configFileName;

            if (!IsUsableFileName(configFileName))
            {
                result.Error = ConfigFileError::NameNotUsable;
                return result;
            }

            auto const folder = FolderPath();

            if (folder.empty())
            {
                result.Error = ConfigFileError::NoFolder;
                return result;
            }

            std::string bytes{};

            result.Error = ReadWholeFile(folder + configFileName, bytes, result.SystemError);

            if (result.Error != ConfigFileError::None)
            {
                return result;
            }

            if (bytes.empty())
            {
                result.Error = ConfigFileError::Empty;
                return result;
            }

            return WriteBackup(folder, configFileName, bytes);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to back up the configuration file.")

        result.Error = ConfigFileError::Unexpected;
        return result;
    }

    _Use_decl_annotations_
    ConfigFileOperationResult AddFromFile(std::wstring const& sourcePath) noexcept
    {
        ConfigFileOperationResult result{};

        try
        {
            result.ConfigFileName = ConfigFileNameForSource(std::wstring{ ::PathFindFileNameW(sourcePath.c_str()) });

            if (!IsUsableFileName(result.ConfigFileName))
            {
                result.Error = ConfigFileError::NameNotUsable;
                return result;
            }

            std::string bytes{};

            result.Error = ReadWholeFile(sourcePath, bytes, result.SystemError);

            if (result.Error != ConfigFileError::None)
            {
                return result;
            }

            if (!LooksLikeConfiguration(bytes))
            {
                result.Error = ConfigFileError::NotAConfiguration;
                return result;
            }

            return PutInPlace(result.ConfigFileName, bytes);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to add the configuration file.")

        result.Error = ConfigFileError::Unexpected;
        return result;
    }

    _Use_decl_annotations_
    ConfigFileOperationResult RestoreBackup(std::wstring const& backupFileName) noexcept
    {
        ConfigFileOperationResult result{};

        try
        {
            if (!IsUsableFileName(backupFileName) ||
                !TryGetBackedUpFileName(backupFileName, result.ConfigFileName))
            {
                result.Error = ConfigFileError::NameNotUsable;
                return result;
            }

            auto const folder = FolderPath();

            if (folder.empty())
            {
                result.Error = ConfigFileError::NoFolder;
                return result;
            }

            std::string bytes{};

            result.Error = ReadWholeFile(folder + backupFileName, bytes, result.SystemError);

            if (result.Error != ConfigFileError::None)
            {
                return result;
            }

            if (!LooksLikeConfiguration(bytes))
            {
                result.Error = ConfigFileError::NotAConfiguration;
                return result;
            }

            return PutInPlace(result.ConfigFileName, bytes);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to restore the configuration backup.")

        result.Error = ConfigFileError::Unexpected;
        return result;
    }
}
