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

#pragma once

#include "ConfigEngine.h"
#include "ConfigPackage.h"

#include <optional>

namespace midisettings::configengine
{
    enum class BackupKind
    {
        // a package MIDI Settings made: the configuration, its pictures and its settings
        Full,

        // the configuration only, made by Windows MIDI Services before the day's first change
        Daily,

        // the configuration only, made by an earlier version of MIDI Settings
        Earlier,
    };

    struct BackupEntry
    {
        std::wstring Path{};
        std::wstring FileName{};
        BackupKind Kind{ BackupKind::Full };

        // the configuration it came from and goes back to
        std::wstring ConfigFileName{};
        std::wstring ConfigName{};

        FILETIME MadeAt{};

        BackupReason Reason{ BackupReason::None };
        std::wstring ReasonComputerName{};
        std::wstring ReasonConfigName{};

        bool IsConfigurationOnly() const noexcept { return Kind != BackupKind::Full; }
    };

    // Full backups from the backups folder and the configuration-only .bak files beside the
    // configurations, newest first.
    std::vector<BackupEntry> ListBackups(_In_ ConfigEnvironment const& environment) noexcept;

    struct BackupResult : ConfigResult
    {
        BackupEntry Backup{};
    };

    // A full backup of a configuration as it is now: the configuration, its pictures and the port
    // names setting. Named <configuration name> <yyyy-MM-dd HHmmss>.midiconfigpack, with -2, -3
    // and so on when that's taken, and never written over another file.
    BackupResult MakeBackup(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configFileName,
        _In_ BackupReason reason,
        _In_ std::wstring const& reasonComputerName,
        _In_ std::wstring const& reasonConfigName) noexcept;

    // The same, of bytes already read, so a backup made before a change holds exactly what the
    // change was planned against.
    BackupResult MakeBackupOfBytes(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configFileName,
        _In_ std::string const& configurationBytes,
        _In_ BackupReason reason,
        _In_ std::wstring const& reasonComputerName,
        _In_ std::wstring const& reasonConfigName) noexcept;

    struct RestoreResult : ConfigResult
    {
        std::wstring ConfigFileName{};

        // what was there before, backed up first so the restore can be undone too
        std::optional<BackupEntry> SafetyBackup{};

        bool Unchanged{ false };
        bool PortNamingChanged{ false };
        uint32_t PicturesStored{ 0 };
        uint32_t PicturesFailed{ 0 };
    };

    // Puts a backup back: the configuration, and for a full backup its pictures and the port
    // names setting. What's there now is backed up first, unless backUpFirst is false, which is
    // how an undo puts back the backup made just before the change it's undoing.
    RestoreResult RestoreBackup(
        _In_ ConfigEnvironment const& environment,
        _In_ BackupEntry const& backup,
        _In_ bool backUpFirst) noexcept;

    ConfigResult DeleteBackup(_In_ BackupEntry const& backup) noexcept;

    // Delete old backups: everything outside the time window, except the newest few of each
    // configuration. Nothing deletes backups by itself.
    std::vector<BackupEntry> OldBackups(
        _In_ std::vector<BackupEntry> const& backups,
        _In_ FILETIME const& now,
        _In_ uint32_t keepDays,
        _In_ uint32_t keepNewestOfEach) noexcept;

    struct DeleteBackupsResult : ConfigResult
    {
        uint32_t Deleted{ 0 };
        uint32_t Failed{ 0 };
    };

    DeleteBackupsResult DeleteBackups(_In_ std::vector<BackupEntry> const& backups) noexcept;

    // The full backup made just before the configuration's last change, when that change was an
    // import or a restore, so the window can say what the change was.
    std::optional<BackupEntry> BackupBeforeLastChange(
        _In_ std::wstring const& configFileName,
        _In_ FILETIME const& lastWritten,
        _In_ std::vector<BackupEntry> const& backups) noexcept;

    // The configuration a .bak file belongs to, from its name: <configuration file>.<stamp>.bak.
    bool TryGetBackedUpFileName(
        _In_ std::wstring_view backupFileName,
        _Out_ std::wstring& configFileName,
        _Out_ BackupKind& kind);
}
