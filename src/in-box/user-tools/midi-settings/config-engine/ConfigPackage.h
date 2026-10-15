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

// A .midiconfigpack: a zip with package.json, configuration.json, settings.json and the pictures
// the configuration uses under pictures/. The format belongs to the in-box MIDI tools. It isn't a
// supported format and can change at any time.

#include "ConfigEngine.h"

#include <span>
#include <utility>

namespace midisettings::configengine
{
    inline constexpr uint32_t PackageFormatVersion{ 1 };

    enum class PackageKind
    {
        Share,
        Backup,
    };

    // Why a backup was made. Kept inside the package, so the file name stays short.
    enum class BackupReason
    {
        None,
        YouBackedItUp,
        BeforeImport,
        BeforeRestore,
        BeforeDelete,
    };

    struct PackageInfo
    {
        uint32_t FormatVersion{ PackageFormatVersion };
        PackageKind Kind{ PackageKind::Share };

        std::wstring ConfigName{};

        // the file a backup goes back to
        std::wstring ConfigFileName{};

        std::wstring Note{};

        // for display, and for spotting network hosts named after that PC
        std::wstring ComputerName{};

        FILETIME Created{};
        bool HasCreated{ false };

        BackupReason Reason{ BackupReason::None };

        // the package being imported, when the reason is BeforeImport
        std::wstring ReasonComputerName{};
        std::wstring ReasonConfigName{};

        std::wstring AppVersion{};

        // the parts inside, as ConfigPart bits
        uint32_t Parts{ 0 };
        uint32_t PictureCount{ 0 };
        bool HasPortNaming{ false };
    };

    struct PackageContents
    {
        PackageInfo Info{};

        // a configuration file saved before packages existed: no pictures and no settings
        bool IsPlainConfiguration{ false };

        std::string ConfigurationBytes{};

        // None when the package doesn't carry the port names setting
        PortNaming PortNamingValue{ PortNaming::None };

        // by the bare name the configuration uses
        std::vector<std::pair<std::wstring, std::vector<uint8_t>>> Pictures{};

        std::vector<uint8_t> const* FindPicture(_In_ std::wstring_view name) const noexcept;
    };

    struct PackageSource
    {
        PackageInfo Info{};
        std::string ConfigurationBytes{};

        // None leaves settings.json out
        PortNaming PortNamingValue{ PortNaming::None };

        // the name the configuration uses, and the full path of the picture file
        std::vector<std::pair<std::wstring, std::wstring>> Pictures{};
    };

    // Written beside the final name and renamed when it's finished, so a failure never leaves
    // half a package where the customer would find it.
    ConfigResult WritePackage(_In_ std::wstring const& path, _In_ PackageSource const& source) noexcept;

    // Reads a package, or a configuration file saved before packages existed. Everything in it is
    // untrusted: it may come from another PC or from the internet. Entries are limited in number
    // and size, only the names above are read, and no name from the zip is ever used as a path.
    ConfigResult ReadPackage(_In_ std::wstring const& path, _Out_ PackageContents& contents) noexcept;
    ConfigResult ReadPackageFromBytes(_In_ std::span<uint8_t const> bytes, _Out_ PackageContents& contents) noexcept;

    // Only package.json, for the list of backups.
    ConfigResult ReadPackageInfo(_In_ std::wstring const& path, _Out_ PackageInfo& info) noexcept;

    // A bare file name with one of the picture types the picture chooser offers.
    bool IsUsablePictureName(_In_ std::wstring_view name) noexcept;
}
